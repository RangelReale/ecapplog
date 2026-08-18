#include "JsonHighlighter.h"

#include <QPalette>

namespace
{
	// The first double quote at or after `from` that is not escaped, or -1. Backslashes are
	// consumed in pairs rather than looked behind, so \\" - an escaped backslash followed by
	// the quote that really does close the string - is read the right way round.
	int closingQuote(const QString &text, int from)
	{
		for (int at = from; at < text.length(); ++at)
		{
			if (text.at(at) == '\\') {
				++at;
				continue;
			}
			if (text.at(at) == '"')
				return at;
		}

		return -1;
	}

	// Where a string literal opens at or after `from` and is still open at the end of the
	// block, or -1 if every literal on the block closes.
	int unterminatedQuote(const QString &text, int from)
	{
		int at = from;
		while (at < text.length())
		{
			if (text.at(at) != '"') {
				++at;
				continue;
			}

			const int close = closingQuote(text, at + 1);
			if (close < 0)
				return at;
			at = close + 1;
		}

		return -1;
	}
}

JsonHighlighter::JsonHighlighter(QTextDocument *document, const QPalette &palette) :
	QSyntaxHighlighter(document)
{
	// Two palettes rather than fixed colours: the app forces the Fusion style on macOS and
	// otherwise follows the system one, so the editor background can be either. #A31515 on a
	// dark base is unreadable, and #CE9178 on a white one is washed out.
	const bool dark = palette.color(QPalette::Base).lightness() < 128;

	auto format = [](const QColor &color) {
		QTextCharFormat fmt;
		fmt.setForeground(color);
		return fmt;
	};

	_stringFormat = format(dark ? QColor("#CE9178") : QColor("#A31515"));

	// Order matters: rules run in sequence and a later one repaints whatever it overlaps.
	// None of these know about context, so the sequence is what supplies it. Punctuation,
	// numbers and keywords go first precisely so that the string rule can paint back over
	// the ones that turn out to be inside a string literal - an escaped payload like
	// "{\"n\": 3}" is one string, and it should not come out speckled with structure
	// colours. Keys go last for the same reason: a key *is* a string, and only the trailing
	// colon tells them apart. That colon is left out of the key format (group 1) so it stays
	// punctuation-coloured.
	_rules.append(Rule{ QRegularExpression("[{}\\[\\],:]"),
		format(dark ? QColor("#808080") : QColor("#7F7F7F")), 0 });
	_rules.append(Rule{ QRegularExpression("-?\\d+(\\.\\d+)?([eE][+-]?\\d+)?"),
		format(dark ? QColor("#B5CEA8") : QColor("#098658")), 0 });
	_rules.append(Rule{ QRegularExpression("\\b(true|false|null)\\b"),
		format(dark ? QColor("#569CD6") : QColor("#0000FF")), 0 });
	_rules.append(Rule{ QRegularExpression("\"(?:\\\\.|[^\"\\\\])*\""), _stringFormat, 0 });
	_rules.append(Rule{ QRegularExpression("(\"(?:\\\\.|[^\"\\\\])*\")\\s*:"),
		format(dark ? QColor("#9CDCFE") : QColor("#0451A5")), 1 });
}

void JsonHighlighter::highlightBlock(const QString &text)
{
	// The state exists for one construct only: the multi-line string values JsonFormat::indented
	// draws across several lines. Every rule above reads one block, and an expanded value is the
	// only thing in these documents that starts on one block and ends on another. Left untracked,
	// the interior lines of an expanded stack trace or escaped payload are just text as far as the
	// punctuation, number and keyword rules can tell, and come out speckled with structure colours
	// - which is the very thing the rule ordering in the constructor exists to prevent.
	//
	// Everything else is still context-free, and none of it needs to be otherwise: outside a
	// string literal, JSON has no construct that spans lines.
	int continuationEnd = 0;
	if (previousBlockState() == StateInString)
	{
		const int close = closingQuote(text, 0);
		if (close < 0)
		{
			// Wholly inside the literal. Nothing else can be on this line, so no rule should
			// see it at all.
			setFormat(0, text.length(), _stringFormat);
			setCurrentBlockState(StateInString);
			return;
		}
		continuationEnd = close + 1;
	}

	for (const Rule &rule : _rules)
	{
		QRegularExpressionMatchIterator iter = rule.pattern.globalMatch(text);
		while (iter.hasNext())
		{
			const QRegularExpressionMatch match = iter.next();
			setFormat(match.capturedStart(rule.group), match.capturedLength(rule.group), rule.format);
		}
	}

	// After the rules, not before, and for the same reason the string rule comes after the
	// structure ones: the tail of the literal this block continues has just been painted as
	// punctuation, numbers and keys by rules with no way of knowing where they were, and
	// painting back over it is what puts that right.
	if (continuationEnd > 0)
		setFormat(0, continuationEnd, _stringFormat);

	// A literal that opens here and does not close is the other half of the same case: the
	// rules cannot match it, because it has no closing quote to match to.
	const int opened = unterminatedQuote(text, continuationEnd);
	if (opened >= 0)
	{
		setFormat(opened, text.length() - opened, _stringFormat);
		setCurrentBlockState(StateInString);
	}
	else
		setCurrentBlockState(StateNormal);
}
