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
