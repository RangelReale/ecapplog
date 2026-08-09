//
// Highlight.h
//

#pragma once

#define NOMINMAX

#include <QString>
#include <QStringList>
#include <QVector>
#include <QPair>

// The words Edit -> Highlight marks in every log list. Matching is case insensitive, and runs
// against the message body alone, which is the same text Find searches.
class Highlight
{
public:
	Highlight() : _words() {}

	const QStringList &words() const { return _words; }
	bool isEmpty() const { return _words.isEmpty(); }

	// false when the word is blank or already listed
	bool add(const QString &word);
	bool remove(const QString &word);
	void clear();

	bool matches(const QString &text) const;

	// Where every word occurs, as (start, length), ordered and with overlaps merged so that two
	// words sharing characters produce one run rather than nested ones.
	QVector<QPair<int, int> > matchRanges(const QString &text) const;
private:
	QStringList _words;
};
