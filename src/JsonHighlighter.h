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

// Colours indented JSON in a QTextEdit. Deliberately rule-based and stateless - see the
// comment on highlightBlock for why that is enough here.
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

	QVector<Rule> _rules;
};
