//
// Data.h
//

# pragma once

#define NOMINMAX

#include "Config.h"
#include "LogModel.h"
#include "Util.h"

#include <QString>
#include <QJsonObject>
#include <QDateTime>
#include <QAbstractListModel>
#include <QStringList>
#include <QTimer>
#include <QElapsedTimer>

#include <memory>
#include <set>
#include <deque>

class Data_Category : public QObject
{
    Q_OBJECT
public:
    Data_Category(const QString &name);

    const QString &name() { return _name; }
    LogModel *model();

    bool addLog(std::shared_ptr<LogModelItem> item);
    void clearLog();
    int checkLogExpiration();
signals:
    void logAmount(const QString& categoryName, int amount);
    void logItemsPerSecond(const QString& categoryName, double itemsPerSecond);
protected:
    int addToModel();
private:
    QString _name;
    LogModel _model;
    QElapsedTimer _elapsed;
    typedef std::deque<std::shared_ptr<LogModelItem> > logs_t;
    logs_t _logs;
    Util::ItemPerSecondChrono<10> _ips;
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
    QStringList categoryNames();
    bool removeCategory(const QString &categoryName);

    void checkLogExpiration();
signals:
    void logAmount(const QString& appName, const QString& categoryName, int amount);
    void logItemsPerSecond(const QString& appName, const QString& categoryName, double itemsPerSecond);
private slots:
    void categoryLogAmount(const QString& categoryName, int amount);
    void categoryLogItemsPerSecond(const QString& categoryName, double itemsPerSecond);
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

    bool toggleFilter(const QString &appName, const QString &categoryName);
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
    ~Data();

    void log(const QString &appName, const QJsonObject &jsonData);
    void log(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
        const QString &message, const QString &source = QString(), const QString &originalCategory = QString(), 
        const QStringList &extraCategories = QStringList(), const LogOptions &logOptions = LogOptions());

    void insertFilter();
    void clearFilter(const QString &filterName);
    void removeApplication(const QString &appName);
    void clearApplication(const QString &appName);
    void clearApplicationLogs(const QString &appName);
    void removeCategory(const QString &appName, const QString &categoryName);
    void clearCategory(const QString& appName, const QString& categoryName);

    void toggleFilter(const QString &filterName, const QString &appName, const QString &categoryName);
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
    void logItemsPerSecond(const QString& appName, const QString& categoryName, double itemsPerSecond);
    void newFilter(const QString &filterName);
    void filterChanged(const QString &filterName);
private slots:
    void checkExpiredLog();
private:
    void logFilter(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
        const QString &message, const QString &source = QString(), const LogOptions& logOptions = LogOptions());
    void internalLog(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
        const QString &message, const QString &source = QString(), const QString &altApp = QString(), 
        const QString &altCategory = QString(), bool isExtraCategory = false, const LogOptions& logOptions = LogOptions());
    void addToModel(std::shared_ptr<Data_Application> application, std::shared_ptr<Data_Category>, 
        const QString& appName, const QDateTime& time, const QString& categoryName, const QString& priority,
        const QString& message, const QString& source, const QString& altApp, const QString& altCategory, bool isExtraCategory,
        const LogOptions& logOptions = LogOptions());

    std::shared_ptr<Data_Application> createApplication(const QString &appName);
    std::shared_ptr<Data_Category> createCategory(std::shared_ptr<Data_Application>, const QString &categoryName);
    std::shared_ptr<Data_Filter> createFilter(const QString &filterName);

    QTimer* _logtimer;
    typedef std::map<QString, std::shared_ptr<Data_Application> > applicationlist_t;    
    typedef std::map<QString, std::shared_ptr<Data_Filter> > filterlist_t;    
    applicationlist_t _applicationlist;
    filterlist_t _filterlist;
    int _filtercount;

    bool _groupCategories;
    bool _paused;
};
