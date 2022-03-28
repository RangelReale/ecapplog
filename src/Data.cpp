#include "Data.h"

Data::Data()
{

}

void Data::log(const QString &appName, const QJsonObject &jsonData)
{
	QString f_category, f_time, f_priority, f_message, f_source;

	if (jsonData.contains("category")) f_category = jsonData.value("category").toString();
	if (jsonData.contains("time")) f_time = jsonData.value("time").toString();
	if (jsonData.contains("priority")) f_priority = jsonData.value("priority").toString();
	if (jsonData.contains("message")) f_message = jsonData.value("message").toString();
	if (jsonData.contains("source")) f_source = jsonData.value("f_source").toString();
    QDateTime time;
	if (!f_time.isEmpty()) {
		// parse time
		QDateTime jsontime = QDateTime::fromString(f_time, "yyyy-MM-ddThh:mm:ss.zzzZ");
		if (jsontime.isValid()) time = jsontime;
	}
	if (time.isNull()) time = QDateTime::currentDateTime();
    if (f_category.isEmpty()) f_category = "<unknown>";

    log(appName, time, f_category, f_priority, f_message, f_source);
}

void Data::log(const QString &appName, const QDateTime &time, const QString &category, const QString &priority,
    const QString &message, const QString &source)
{

}
