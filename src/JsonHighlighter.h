//
// JsonHighlighter.h
//

#pragma once

#define NOMINMAX

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QVector>

class QPalette;

// Colours indented JSON in a QTextEdit. Rule-based, with exactly one bit of state: the
// documents come from JsonFormat::indented, which draws a multi-line string value across
// several lines, so a string literal can straddle blocks - see highlightBlock.
class JsonHighlighter : public QSyntaxHighlighter
{
	Q_OBJECT
public:
	JsonHighlighter(QTextDocument *document, const QPalette &palette);
protected:
	void highlightBlock(const QString &text) override;
private:
	struct Rule
	{
		QRegularExpression pattern;
		QTextCharFormat    format;
		int                group;	// capture group to paint; 0 is the whole match
	};

	// Whether the block after this one opens inside a string literal. QSyntaxHighlighter
	// reports -1 for "no state", the first block included, so StateNormal must not be -1.
	enum State
	{
		StateNormal   = 0,
		StateInString = 1
	};

	QVector<Rule> _rules;

	// Held apart from the rule that uses it, because the parts of a straddling literal that
	// no single-block pattern can match are painted with it directly.
	QTextCharFormat _stringFormat;
};
