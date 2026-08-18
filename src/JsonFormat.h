//
// JsonFormat.h
//

#pragma once

#define NOMINMAX

#include <QJsonDocument>
#include <QString>

// Turns a parsed document into the text the detail window shows. Separate from JsonScan,
// which is the other half of the same job: JsonScan finds JSON inside a log line, this
// decides what the found JSON looks like on screen.
namespace JsonFormat
{
	// toJson(Indented), except that a string value which really spans lines is drawn
	// spanning lines instead of carrying its newlines as \n escapes.
	//
	// The result is therefore NOT valid JSON - a raw newline inside a string literal is not
	// legal - and nothing may feed it back to a parser. It is display text. JsonHighlighter
	// knows about it, because expanded strings straddle text blocks and it has to track that.
	QString indented(const QJsonDocument &doc);
}
