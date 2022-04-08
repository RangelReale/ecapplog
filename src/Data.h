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
#include <QStringList>

#include <memory>
#include <set>

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
    bool groupCategories() const { return _groupCategories; }
    void setGroupCategories(bool value) { _groupCategories = value; }

    void addCategory(std::shared_ptr<Data_Category> category);
    std::shared_ptr<Data_Category> findCategory(const QString &categoryName);
    bool removeCategory(const QString &categoryName);
private:
    QString _name;
    bool _groupCategories;

    typedef std::map<QString, std::shared_ptr<Data_Category> > categorylist_t;
    categorylist_t _categorylist;
};

class Data_FilterItem : public QObject
{
    Q_OBJECT
public:
    Data_FilterItem(const QString &application, const QString &category);

    QString application;
    QString category;
};

enum class Data_Filter_GroupBy
{
    All,
    ByCategory,
    ByApplication
};

class Data_Filter : public QObject
{
    Q_OBJECT
public:
    Data_Filter(const QString &name);

    const QString &name() { return _name; }
    Data_Filter_GroupBy groupBy() const { return _groupby; }
    void setGroupBy(Data_Filter_GroupBy value) { _groupby = value; }

    void toogleFilter(const QString &appName, const QString &categoryName);
    void clearFilter();
    bool isFilter(const QString &appName, const QString &categoryName);
    QString filterCategoryName(const QString &appName, const QString &categoryName);
private:
    QString parseAppName(const QString &appName);

    QString _name;
    Data_Filter_GroupBy _groupby;
    typedef std::set<QString> filterlist_t;
    filterlist_t _filterlist;
};

class Data : public QObject
{
    Q_OBJECT
public:
    Data();

    void log(const QString &appName, const QJsonObject &jsonData);
    void log(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
        const QString &message, const QString &source = QString(), const QString &originalCategory = QString(), 
        const QStringList &extraCategories = QStringList());

    void insertFilter();
    void clearFilter(const QString &filterName);
    void removeApplication(const QString &appName);
    void removeCategory(const QString &appName, const QString &categoryName);

    void toogleFilter(const QString &filterName, const QString &appName, const QString &categoryName);
    void setFilterGroupBy(const QString &filterName, Data_Filter_GroupBy groupby);

    void removeAllApplications();

    bool getGroupCategories() const;
    void setGroupCategories(bool value);

    bool getApplicationGroupCategories(const QString &appName) const;
    void setApplicationGroupCategories(const QString &appName, bool value);

    bool getPaused() const;
    void setPaused(bool value);
    QStringList filterNames() const;
signals:
    void newApplication(const QString &appName);
    void delApplication(const QString &appName);
    void newCategory(const QString &appName, const QString &categoryName, QAbstractListModel *model);
    void delCategory(const QString &appName, const QString &categoryName);
    void logAmount(const QString &appName, const QString &categoryName, int amount);
    void newFilter(const QString &filterName);
    void filterChanged(const QString &filterName);
private:
    void logFilter(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
        const QString &message, const QString &source = QString());
    void internalLog(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
        const QString &message, const QString &source = QString(), const QString &altApp = QString(), 
        const QString &altCategory = QString(), bool isExtraCategory = false);
    void addToModel(LogModel *model, const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
        const QString &message, const QString &source, const QString &altApp, const QString &altCategory, bool isExtraCategory);

    std::shared_ptr<Data_Application> createApplication(const QString &appName);
    std::shared_ptr<Data_Category> createCategory(std::shared_ptr<Data_Application>, const QString &categoryName);
    std::shared_ptr<Data_Filter> createFilter(const QString &filterName);

    typedef std::map<QString, std::shared_ptr<Data_Application> > applicationlist_t;    
    typedef std::map<QString, std::shared_ptr<Data_Filter> > filterlist_t;    
    applicationlist_t _applicationlist;
    filterlist_t _filterlist;
    int _filtercount;

    bool _groupCategories;
    bool _paused;
};
