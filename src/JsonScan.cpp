#include "JsonScan.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

// A log line with a lot of stray braces makes the literal scan quadratic (every '{' costs a
// walk to the end of the text looking for a close that never comes), and "Detail..." can be
// opened on a whole multi-row selection. Past this size the window just shows the text.
#define JSONSCAN_MAX_TEXT		(256 * 1024)

// A pathological entry - a big array of stringified objects, say - would otherwise fill the
// tab bar with dozens of tabs nobody is going to click.
#define JSONSCAN_MAX_MATCHES	32

namespace {

// Index one past the bracket that closes the one at `start`, or -1 if it never closes.
// String literals are skipped so that braces inside them do not affect the depth. Whether
// the closing bracket is the right *kind* is not checked here - QJsonDocument rejects
// "{ ... ]" anyway, and letting it do that keeps this loop simple.
int matchBalanced(const QString &text, int start)
{
	int depth = 0;
	bool inString = false;

	for (int i = start; i < text.length(); i++)
	{
		const QChar c = text.at(i);

		if (inString) {
			if (c == '\\') {
				i++;			// escaped character, whatever it is, is not a quote
				continue;
			}
			if (c == '"') inString = false;
			continue;
		}

		if (c == '"') {
			inString = true;
		} else if (c == '{' || c == '[') {
			depth++;
		} else if (c == '}' || c == ']') {
			depth--;
			if (depth == 0) return i + 1;
			if (depth < 0) return -1;
		}
	}

	return -1;
}

// Is this value worth a tab of its own? Valid JSON is not the same question.
//
// An empty {} or [] parses fine but a tab showing "{}" tells nobody anything, and stray empty
// braces in prose are common enough to matter. (QJsonDocument::isEmpty() is no help - in Qt5
// it only repeats isNull() - so the container has to be asked directly.)
//
// Arrays need a stricter test still, because bracketed numbers are everywhere in log output
// and every one of them is a valid array: `DownloadFileToMemory[11]` would otherwise get a
// tab containing 11, and so would every index, count and thread id in the stream. An array
// whose every element is an object is structured data somebody meant to send; anything else
// is almost always coincidence. A deliberate ["a","b"] is the price, and it can still be read
// by selecting it and using "Try parsing JSON", which does not go through here.
bool isTabWorthy(const QJsonDocument &doc)
{
	if (doc.isArray()) {
		const QJsonArray arr = doc.array();
		if (arr.isEmpty()) return false;

		// at() rather than a range-for: Qt5's iterator hands out QJsonValueRef
		for (int i = 0; i < arr.size(); i++)
			if (!arr.at(i).isObject()) return false;

		return true;
	}

	return !doc.object().isEmpty();
}

// Pass A: complete JSON values sitting literally in the text. Returns how many characters of
// `text` they account for, which is what findAll() uses to judge one reading of the text
// against another.
int scanLiteral(const QString &text, QVector<JsonScan::Match> &out)
{
	int covered = 0;
	int i = 0;

	while (i < text.length() && out.size() < JSONSCAN_MAX_MATCHES)
	{
		const QChar c = text.at(i);
		if (c == '{' || c == '[') {
			const int end = matchBalanced(text, i);
			if (end > i) {
				const QJsonDocument doc = JsonScan::tryParse(text.mid(i, end - i));
				if (!doc.isNull() && isTabWorthy(doc)) {
					out.append(JsonScan::Match{ doc, QString() });
					covered += end - i;
					// resume *after* the value, so nested sub-objects don't each become a
					// match of their own - the whole point is one tab per complete value
					i = end;
					continue;
				}
			}
		}
		i++;
	}

	return covered;
}

// Pass B: JSON carried escaped inside a string value, the `"payload":"{\"a\":1}"` shape.
//
// This is done on the parsed tree rather than on the raw text because the parser has already
// undone the escaping for us. Trying to bracket-match `{\"a\":1}` in place does not work:
// the string-skipping in matchBalanced sees the backslash before the closing quote, treats
// it as an escape, and never finds the end of the string.
void scanEmbedded(const QJsonValue &value, const QString &path, QVector<JsonScan::Match> &out)
{
	if (out.size() >= JSONSCAN_MAX_MATCHES) return;

	if (value.isObject()) {
		const QJsonObject obj = value.toObject();
		for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
			scanEmbedded(it.value(), path.isEmpty() ? it.key() : path + "." + it.key(), out);
	} else if (value.isArray()) {
		const QJsonArray arr = value.toArray();
		for (int i = 0; i < arr.size(); i++)
			scanEmbedded(arr.at(i), QString("%1[%2]").arg(path).arg(i), out);
	} else if (value.isString()) {
		const QString str = value.toString().trimmed();
		if (str.startsWith('{') || str.startsWith('[')) {
			const QJsonDocument doc = JsonScan::tryParse(str);
			if (!doc.isNull() && isTabWorthy(doc)) {
				out.append(JsonScan::Match{ doc, path });
				// an unwrapped payload can itself wrap another one; this terminates because
				// each unwrap works on a strict substring of what it came from
				scanEmbedded(doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object()),
					path, out);
			}
		}
	}
}

}	// namespace

QVector<JsonScan::Match> JsonScan::findAll(const QString &text)
{
	QVector<Match> result;

	if (text.length() > JSONSCAN_MAX_TEXT) return result;

	QVector<Match> literal;
	const int literalCovered = scanLiteral(text, literal);

	// Pass C: the line may itself be one escaped blob with no valid literal wrapper around it -
	// `raw: {\"escaped\":\"blob\",\"k\":[1,2,3]}`. The literal pass cannot read that (the
	// backslash before each quote makes every string look unterminated) but it does not come
	// back empty-handed either: it picks off the one fragment that happens to be unescaped,
	// here the [1,2,3]. So the two readings are compared on how much of the text they account
	// for rather than on whether the first found anything, and the ties go to the literal one.
	if (text.contains('\\')) {
		QString unquoted(text);
		unquoted.replace(QRegularExpression("\\\\(.)", QRegularExpression::DotMatchesEverythingOption), "\\1");
		if (unquoted != text) {
			QVector<Match> unescaped;
			if (scanLiteral(unquoted, unescaped) > literalCovered)
				literal = unescaped;
		}
	}

	// Emit each literal value followed by whatever it carries, so a payload tab sits next to
	// the object it was pulled out of.
	for (const Match &match : literal)
	{
		if (result.size() >= JSONSCAN_MAX_MATCHES) break;
		result.append(match);
		scanEmbedded(match.doc.isArray() ? QJsonValue(match.doc.array()) : QJsonValue(match.doc.object()),
			QString(), result);
	}

	return result;
}

QJsonDocument JsonScan::tryParse(const QString &text)
{
	// fromJson() wants UTF-8. toLocal8Bit(), which this used to do, silently mangles anything
	// outside the system codepage - exactly the sort of payload the detail window is opened for.
	QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
	if (!doc.isNull()) return doc;

	// try unquoting
	QString unquoted(text);
	unquoted.replace(QRegularExpression("\\\\(.)", QRegularExpression::DotMatchesEverythingOption), "\\1");

	return QJsonDocument::fromJson(unquoted.toUtf8());
}
