#include "JsonFormat.h"

#include <QStringList>
#include <QVector>

namespace
{
	// What toJson(Indented) indents one level by. Continuation lines are built from it, so if
	// Qt ever changed its step the expanded block would drift out of line with the structure
	// around it - it would still read, but it would no longer sit under its key.
	const int INDENT_STEP = 4;

	// One newline escape inside a string literal: where it starts and how long it is, \r\n
	// being twice the length of \n.
	struct Break
	{
		int at;
		int len;
	};

	// Every newline escape in one string literal's contents, left to right.
	//
	// Escapes are consumed in pairs rather than searched for, which is the whole point: the n
	// in \\n is a literal n after a literal backslash, and a plain indexOf("\\n") would read
	// it as a newline and break a value that spells out a Windows path or a regex.
	QVector<Break> newlineEscapes(const QString &content)
	{
		QVector<Break> breaks;

		for (int i = 0; i < content.size(); )
		{
			if (content.at(i) != '\\') {
				++i;
				continue;
			}

			if (i + 1 >= content.size())
				break;	// a lone trailing backslash; toJson never writes one, so nothing to do

			const QChar esc = content.at(i + 1);
			if (esc == 'n') {
				breaks.append(Break{ i, 2 });
				i += 2;
			} else if (esc == 'r' && i + 3 < content.size()
					&& content.at(i + 2) == '\\' && content.at(i + 3) == 'n') {
				// taken as one break, before the \n it ends in is seen on its own, or every
				// expanded line of a CRLF value would trail a stray \r escape
				breaks.append(Break{ i, 4 });
				i += 4;
			} else {
				// Any other escape, \\ included, and skipping its second character is the
				// part that matters - see above.
				//
				// A lone \r is deliberately one of these. It is as likely to be a progress
				// line rewriting itself in place as it is to be a line ending, and breaking
				// on it would turn one line of intended output into several.
				i += 2;
			}
		}

		return breaks;
	}

	// True if the string literal ending at `close` is an object key, which is to say the next
	// thing on the line is the colon that separates it from its value.
	//
	// Keys are left alone. Not because a key with a newline in it is impossible - it is legal
	// JSON - but because breaking one would put the colon on a line of its own, and the
	// highlighter tells a key from a plain string by finding that colon behind it.
	bool isKey(const QString &line, int close)
	{
		int at = close + 1;
		while (at < line.size() && line.at(at) == ' ')
			++at;

		return at < line.size() && line.at(at) == ':';
	}

	// The contents of one string literal, with each newline escape replaced by a real line
	// break and the indent that puts the next line under the value it belongs to.
	QString expand(const QString &content, const QString &continuation, const QVector<Break> &breaks)
	{
		QString out;
		out.reserve(content.size() + breaks.size() * (continuation.size() + 1));

		int from = 0;
		for (const Break &brk : breaks)
		{
			out.append(content.mid(from, brk.at - from));
			out.append('\n');
			out.append(continuation);
			from = brk.at + brk.len;
		}
		out.append(content.mid(from));

		return out;
	}

	// Rewrites toJson(Indented) output, expanding the string values that span lines.
	//
	// A line at a time, and that is safe only because of what toJson(Indented) guarantees:
	// one value per line, and every string literal opened and closed on the line it starts
	// on. Neither holds for JSON in general, and this must not be pointed at text from
	// anywhere else.
	QString expandMultilineStrings(const QString &json)
	{
		QString out;
		out.reserve(json.size());

		const QStringList lines = json.split('\n');
		for (int l = 0; l < lines.size(); ++l)
		{
			const QString &line = lines.at(l);
			if (l > 0)
				out.append('\n');

			// The value's own line is indented for its depth; its continuation lines go one
			// level further in, so the block reads as belonging to the key above it rather
			// than as another member of the object.
			int indent = 0;
			while (indent < line.size() && line.at(indent) == ' ')
				++indent;
			const QString continuation(indent + INDENT_STEP, ' ');

			int at = 0;
			while (at < line.size())
			{
				const int open = line.indexOf('"', at);
				if (open < 0) {
					out.append(line.mid(at));
					break;
				}

				int close = open + 1;
				while (close < line.size())
				{
					if (line.at(close) == '\\') {
						close += 2;
						continue;
					}
					if (line.at(close) == '"')
						break;
					++close;
				}
				if (close >= line.size()) {
					// Unreachable for toJson output, and passed through rather than asserted:
					// this is a formatter, and a line it cannot read is still a line to show.
					out.append(line.mid(at));
					break;
				}

				const QString content = line.mid(open + 1, close - open - 1);
				const QVector<Break> breaks = newlineEscapes(content);

				// Only a value with a break somewhere other than its end is multi-line. A
				// value that merely ends in \n - a log message keeping the newline it was
				// printed with - is one line, and expanding it would gain nothing but a
				// closing quote stranded on a line of its own.
				const bool multiline = !breaks.isEmpty()
						&& !(breaks.size() == 1 && breaks.first().at + breaks.first().len == content.size());

				out.append(line.mid(at, open + 1 - at));
				out.append(multiline && !isKey(line, close)
						? expand(content, continuation, breaks) : content);
				out.append('"');

				at = close + 1;
			}
		}

		return out;
	}
}

QString JsonFormat::indented(const QJsonDocument &doc)
{
	// Qt formats, we only rewrite the strings afterwards. Walking the document ourselves would
	// mean writing every value out by hand, and QString::number does not reproduce toJson's
	// doubles - a rewrite of the finished text leaves every number exactly as it was.
	return expandMultilineStrings(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
}
