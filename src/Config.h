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

// The two categories the application creates by itself, rather than taking from a log entry: ALL
// aggregates every category of an application when View -> Group categories is on, and ERROR
// collects the error and warning lines of all of them. Named because the routing in Data and the
// tab ordering in MainWindow have to agree on them exactly.
class Category
{
public:
	static const QString CAT_ALL;
	static const QString CAT_ERROR;
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
