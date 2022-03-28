//
// Data.h
//

# pragma once

#define NOMINMAX

#include "Config.h"
#include "LogModel.h"

#include <QString>
#include <QJsonObject>
#include <QDateTime>
#include <QAbstractListModel>

#include <memory>

class Data_Category : public QObject
{
    Q_OBJECT
public:
    Data_Category(const QString &name);

    const QString &name() { return _name; }
    LogModel *model();
private:
    QString _name;
    LogModel _model;
};

class Data_Application : public QObject
{
    Q_OBJECT
public:
    Data_Application(const QString &name);

    const QString &name() { return _name; }

    void addCategory(std::shared_ptr<Data_Category> category);
    std::shared_ptr<Data_Category> findCategory(const QString &categoryName);
    bool removeCategory(const QString &categoryName);
private:
    QString _name;

    typedef std::map<QString, std::shared_ptr<Data_Category> > categorylist_t;
    categorylist_t _categorylist;
};

class Data : public QObject
{
    Q_OBJECT
public:
    Data();

    void log(const QString &appName, const QJsonObject &jsonData);
    void log(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
        const QString &message, const QString &source = QString());

    void removeApplication(const QString &appName);
    void removeCategory(const QString &appName, const QString &categoryName);

    void removeAllApplications();
signals:
    void newApplication(const QString &appName);
    void delApplication(const QString &appName);
    void newCategory(const QString &appName, const QString &categoryName, QAbstractListModel *model);
    void delCategory(const QString &appName, const QString &categoryName);
    void logAmount(const QString &appName, const QString &categoryName, int amount);
private:
    void internalLog(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
        const QString &message, const QString &source = QString(), const QString &altApp = QString(), 
        const QString &altCategory = QString());
    void addToModel(LogModel *model, const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
        const QString &message, const QString &source, const QString &altApp, const QString &altCategory);

    std::shared_ptr<Data_Application> createApplication(const QString &appName);
    std::shared_ptr<Data_Category> createCategory(std::shared_ptr<Data_Application>, const QString &categoryName);

    typedef std::map<QString, std::shared_ptr<Data_Application> > applicationlist_t;    
    applicationlist_t _applicationlist;
};
