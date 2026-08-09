#include "Highlight.h"

#include <algorithm>

bool Highlight::add(const QString &word)
{
	QString trimmed = word.trimmed();
	if (trimmed.isEmpty()) return false;

	for (const QString &existing : _words)
	{
		if (existing.compare(trimmed, Qt::CaseInsensitive) == 0) return false;
	}

	_words.append(trimmed);
	return true;
}

bool Highlight::remove(const QString &word)
{
	for (int i = 0; i < _words.size(); ++i)
	{
		if (_words.at(i).compare(word, Qt::CaseInsensitive) == 0)
		{
			_words.removeAt(i);
			return true;
		}
	}
	return false;
}

void Highlight::clear()
{
	_words.clear();
}

bool Highlight::matches(const QString &text) const
{
	for (const QString &word : _words)
	{
		if (text.contains(word, Qt::CaseInsensitive)) return true;
	}
	return false;
}

QVector<QPair<int, int> > Highlight::matchRanges(const QString &text) const
{
	QVector<QPair<int, int> > ranges;
	if (_words.isEmpty() || text.isEmpty()) return ranges;

	for (const QString &word : _words)
	{
		int from = 0;
		while (true)
		{
			int at = text.indexOf(word, from, Qt::CaseInsensitive);
			if (at < 0) break;
			ranges.append(qMakePair(at, word.length()));
			from = at + 1;	// +1 rather than +length, so overlapping repeats are not skipped
		}
	}

	if (ranges.size() < 2) return ranges;

	std::sort(ranges.begin(), ranges.end());

	QVector<QPair<int, int> > merged;
	merged.append(ranges.first());
	for (int i = 1; i < ranges.size(); ++i)
	{
		QPair<int, int> &last = merged.last();
		const QPair<int, int> &range = ranges.at(i);
		int lastEnd = last.first + last.second;
		if (range.first <= lastEnd)
		{
			last.second = qMax(lastEnd, range.first + range.second) - last.first;
		}
		else
		{
			merged.append(range);
		}
	}

	return merged;
}
