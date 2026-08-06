//
// Config.h
//

# pragma once

#define NOMINMAX

#include <QString>
#include <QWidget>

class Priority
{
public:
	static const QString PRIO_TRACE;
	static const QString PRIO_DEBUG;
	static const QString PRIO_INFORMATION;
	static const QString PRIO_NOTICE;
	static const QString PRIO_WARNING;
	static const QString PRIO_FATAL;
	static const QString PRIO_CRITICAL;
	static const QString PRIO_ERROR;

	static bool isError(const QString &p);
	static bool isErrorOrWarning(const QString &p);
};

enum Command
{
	CMD_BANNER		= 99,
	CMD_LOG			= 0,
};

// Ceiling on a single protocol frame. Without it a hostile or corrupt length prefix is a remote
// memory exhaustion, and any value above INT_MAX overflows the int parameter of
// QByteArray::resize before allocation is even attempted.
#define MAX_PAYLOAD_SIZE (16 * 1024 * 1024)

#define PROPERTY_APPNAME "ecapplog_application"
#define PROPERTY_CATEGORYNAME "ecapplog_category"
