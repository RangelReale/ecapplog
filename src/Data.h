//
// Data.h
//

# pragma once

#define NOMINMAX

#include "Config.h"

#include <QString>
#include <QJsonObject>
#include <QDateTime>

class Data
{
public:
    Data();

    void log(const QString &appName, const QJsonObject &jsonData);
    void log(const QString &appName, const QDateTime &time, const QString &category, const QString &priority,
        const QString &message, const QString &source = QString());
};
