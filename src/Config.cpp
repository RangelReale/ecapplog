#include "Config.h"

const QString Priority::PRIO_TRACE = QString("TRACE");
const QString Priority::PRIO_DEBUG = QString("DEBUG");
const QString Priority::PRIO_INFORMATION = QString("INFORMATION");
const QString Priority::PRIO_NOTICE = QString("NOTICE");
const QString Priority::PRIO_WARNING = QString("WARNING");
const QString Priority::PRIO_FATAL = QString("FATAL");
const QString Priority::PRIO_CRITICAL = QString("CRITICAL");
const QString Priority::PRIO_ERROR = QString("ERROR");

bool Priority::isError(const QString &p)
{
	return p == PRIO_FATAL || p == PRIO_CRITICAL || p == PRIO_ERROR;
}

bool Priority::isErrorOrWarning(const QString &p)
{
	return isError(p) || p == PRIO_WARNING;
}
