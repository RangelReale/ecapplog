#include "JsonHighlighter.h"

#include <QPalette>

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
	_rules.append(Rule{ QRegularExpression("\"(?:\\\\.|[^\"\\\\])*\""),
		format(dark ? QColor("#CE9178") : QColor("#A31515")), 0 });
	_rules.append(Rule{ QRegularExpression("(\"(?:\\\\.|[^\"\\\\])*\")\\s*:"),
		format(dark ? QColor("#9CDCFE") : QColor("#0451A5")), 1 });
}

void JsonHighlighter::highlightBlock(const QString &text)
{
	// No previousBlockState()/setCurrentBlockState() bookkeeping, and none is needed: every
	// document this is attached to was produced by QJsonDocument::toJson(Indented), so it is
	// well-formed and every string literal is on one line with its newlines written as \n.
	// A string can never straddle a block, which is the only thing that would need state.
	for (const Rule &rule : _rules)
	{
		QRegularExpressionMatchIterator iter = rule.pattern.globalMatch(text);
		while (iter.hasNext())
		{
			const QRegularExpressionMatch match = iter.next();
			setFormat(match.capturedStart(rule.group), match.capturedLength(rule.group), rule.format);
		}
	}
}
