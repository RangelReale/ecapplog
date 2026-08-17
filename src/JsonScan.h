//
// JsonScan.h
//

#pragma once

#define NOMINMAX

#include <QJsonDocument>
#include <QString>
#include <QVector>

// Finds complete JSON values embedded in arbitrary log text. Log lines rarely *are* JSON;
// they usually carry one or more JSON values somewhere inside a prose message, and very
// often carry a second one escaped inside a string value of the first.
namespace JsonScan
{
	struct Match
	{
		QJsonDocument doc;
		QString       path;	// empty for a value found directly in the text; otherwise the
							// key path of the string it was unwrapped from ("payload",
							// "data.body"), which is what makes a useful tab label
	};

	// Every complete JSON value in `text`, outermost first, in the order they start.
	QVector<Match> findAll(const QString &text);

	// Parse a single value, retrying once with backslash escapes stripped so that a blob
	// copied out of an already-escaped log line still parses.
	QJsonDocument tryParse(const QString &text);
}
