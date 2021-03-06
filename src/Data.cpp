#include "Data.h"

#include <QJsonArray>

Data::Data() : _applicationlist(), _filterlist(), _filtercount(0), _groupCategories(false), _paused(false)
{
    _logtimer = new QTimer(this);
    connect(_logtimer, &QTimer::timeout, this, &Data::checkExpiredLog);
    _logtimer->start(1000);
}

Data::~Data()
{
    _logtimer->stop();
}

void Data::log(const QString &appName, const QJsonObject &jsonData)
{
	QString f_category, f_time, f_priority, f_message, f_source, f_originalCategory;
    QStringList f_extraCategories;
    LogOptions f_logOptions;

	if (jsonData.contains("category")) f_category = jsonData.value("category").toString();
	if (jsonData.contains("time")) f_time = jsonData.value("time").toString();
	if (jsonData.contains("priority")) f_priority = jsonData.value("priority").toString();
	if (jsonData.contains("message")) f_message = jsonData.value("message").toString();
	if (jsonData.contains("source")) f_source = jsonData.value("source").toString();
	if (jsonData.contains("original_category")) f_originalCategory = jsonData.value("original_category").toString();
	if (jsonData.contains("extra_categories")) {
        auto catJson = jsonData.value("extra_categories");
        if (catJson.isArray()) {
            for (auto extraCat : catJson.toArray()) f_extraCategories.push_back(extraCat.toString());
        } else if (catJson.isString()) {
            f_extraCategories.push_back(catJson.toString());
        }
    }
    if (jsonData.contains("color")) f_logOptions.color = jsonData.value("color").toString();
    if (jsonData.contains("bgcolor")) f_logOptions.bgColor = jsonData.value("bgcolor").toString();

    QDateTime time;
	if (!f_time.isEmpty()) {
		// parse time
		QDateTime jsontime = QDateTime::fromString(f_time, "yyyy-MM-ddThh:mm:ss.zzz");
		if (jsontime.isValid()) {
            jsontime.setTimeSpec(Qt::UTC);
            time = jsontime;
        }
	}
	if (time.isNull()) time = QDateTime::currentDateTime();

    log(appName, time, f_category, f_priority, f_message, f_source, f_originalCategory, f_extraCategories, f_logOptions);
}

void Data::log(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    const QString &message, const QString &source, const QString &originalCategory, const QStringList &extraCategories,
    const LogOptions& logOptions)
{
    QString logCategory(categoryName);
    QString altCategory;
    QString categoryNameComplete(categoryName);
    if (!originalCategory.isEmpty()) {
        altCategory = originalCategory;
        categoryNameComplete = QString("%1 [%2]").arg(categoryName, originalCategory);
    }
    bool groupCategories = _groupCategories;
    if (!groupCategories)
    {
        auto findapp = _applicationlist.find(appName);
        if (findapp != _applicationlist.end())
        {
            groupCategories = findapp->second->groupCategories();
        }
    }
    if (groupCategories) 
    {
        altCategory = categoryNameComplete;
        logCategory = "ALL";
    }

    internalLog(appName, time, logCategory, priority, message, source, "", altCategory, false, logOptions);
    logFilter(appName, time, logCategory, priority, message, source);
    if (Priority::isErrorOrWarning(priority))
    {
        internalLog(appName, time, "ERROR", priority, message, source, "", categoryNameComplete, false, logOptions);
    }

    if (!extraCategories.isEmpty())
    {
        for ( const auto& extraCategory : extraCategories  )
        {
            if (extraCategory != logCategory) {
                internalLog(appName, time, extraCategory, priority, message, source, "", categoryNameComplete, true, logOptions);
                logFilter(appName, time, extraCategory, priority, message, source);
            }
        }
    }
}

void Data::logFilter(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    const QString &message, const QString &source, const LogOptions& logOptions)
{
    for (auto filter : _filterlist)
    {
        if (filter.second->isFilter(appName, categoryName))
        {
            internalLog(filter.second->name(), time, filter.second->filterCategoryName(appName, categoryName), 
                priority, message, source, appName, categoryName, false, logOptions);
        }
    }
}

void Data::internalLog(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    const QString &message, const QString &source, const QString &altApp, const QString &altCategory, bool isExtraCategory,
    const LogOptions& logOptions)
{
    if (_paused) return;

    QString appCategory(categoryName);
    if (appCategory.isEmpty()) appCategory = "<unknown>";

    // find application
    std::shared_ptr<Data_Application> app;
    auto findapp = _applicationlist.find(appName);
    if (findapp == _applicationlist.end())
    {
        app = createApplication(appName);
    }
    else
        app = findapp->second;

    // find category
    auto category = app->findCategory(appCategory);
    if (!category)
    {
        category = createCategory(app, appCategory);
    }

    addToModel(app, category, appName, time, appCategory, priority, message, source, altApp, altCategory, isExtraCategory, logOptions);
}

void Data::addToModel(std::shared_ptr<Data_Application> application, std::shared_ptr<Data_Category> category,
    const QString& appName, const QDateTime& time,
    const QString& categoryName, const QString& priority, const QString& message, const QString& source, 
    const QString& altApp, const QString& altCategory, bool isExtraCategory, const LogOptions& logOptions)
{
    category->addLog(std::make_shared<LogModelItem>(appName, time, categoryName, priority, message, source,
        altApp, altCategory, isExtraCategory, logOptions));
}

void Data::removeApplication(const QString &appName)
{
    if (_applicationlist.find(appName) == _applicationlist.end()) return;
    _applicationlist.erase(appName);
    emit delApplication(appName);
}

void Data::clearApplication(const QString &appName)
{
    auto app = _applicationlist.find(appName);
    if (app == _applicationlist.end()) return;

    for (auto category : app->second->categoryNames())
    {
        removeCategory(appName, category);
    }
}

void Data::clearApplicationLogs(const QString &appName)
{
    auto app = _applicationlist.find(appName);
    if (app == _applicationlist.end()) return;

    for (auto category : app->second->categoryNames())
    {
        clearCategory(appName, category);
    }
}

void Data::removeAllApplications()
{
    for (auto app : _applicationlist)
    {
        emit delApplication(app.second->name());
    }
    _applicationlist.clear();
}

void Data::removeCategory(const QString &appName, const QString &categoryName)
{
    auto app = _applicationlist.find(appName);
    if (app == _applicationlist.end()) return;

    if (app->second->removeCategory(categoryName)) {
        emit delCategory(appName, categoryName);
    }
}

void Data::clearCategory(const QString& appName, const QString& categoryName)
{
    auto app = _applicationlist.find(appName);
    if (app == _applicationlist.end()) return;
    auto category = app->second->findCategory(categoryName);
    if (!category) return;

    category->clearLog();
}

void Data::insertFilter()
{
    QString filterName = QString("FILTER%1").arg(++_filtercount);
    createFilter(filterName);
}

void Data::clearFilter(const QString &filterName)
{
    auto findfilter = _filterlist.find(filterName);
    if (findfilter == _filterlist.end()) return;

    findfilter->second->clearFilter();
}

std::shared_ptr<Data_Application> Data::createApplication(const QString &appName)
{
    auto app = std::make_shared<Data_Application>(appName);
    connect(app.get(), &Data_Application::logAmount, this, &Data::logAmount);
    connect(app.get(), &Data_Application::logItemsPerSecond, this, &Data::logItemsPerSecond);
    _applicationlist[appName] = app;
    emit newApplication(appName);
    return app;
}

std::shared_ptr<Data_Category> Data::createCategory(std::shared_ptr<Data_Application> app, const QString &categoryName)
{
    auto category = std::make_shared<Data_Category>(categoryName);
    app->addCategory(category);
    emit newCategory(app->name(), categoryName, category->model());
    return category;
}

std::shared_ptr<Data_Filter> Data::createFilter(const QString &filterName)
{
    auto filter = std::make_shared<Data_Filter>(filterName);
    _filterlist[filterName] = filter;
    emit newFilter(filterName);
    return filter;
}

void Data::toggleFilter(const QString &filterName, const QString &appName, const QString &categoryName)
{
    auto findfilter = _filterlist.find(filterName);
    if (findfilter == _filterlist.end()) return;

    bool added = findfilter->second->toggleFilter(appName, categoryName);
    QString priority(Priority::PRIO_INFORMATION);
    if (!added) priority = Priority::PRIO_TRACE;

    QString filterData;
    if (categoryName.isEmpty())
    {
        filterData = appName;
    } 
    else 
    {
        filterData = QString("%1 - %2").arg(appName).arg(categoryName);
    }

    internalLog(findfilter->second->name(), QDateTime::currentDateTime(), "FILTER", priority,
        QString("'%1' %2 filter").arg(filterData).arg(added?"added to":"removed from"));
}

void Data::setFilterGroupBy(const QString &filterName, Data_Filter_GroupBy groupby)
{
    auto findfilter = _filterlist.find(filterName);
    if (findfilter == _filterlist.end()) return;

    findfilter->second->setGroupBy(groupby);
    emit filterChanged(filterName);
}

bool Data::getGroupCategories() const
{
    return _groupCategories;
}

void Data::setGroupCategories(bool value)
{
    _groupCategories = value;
}

bool Data::getApplicationGroupCategories(const QString &appName) const
{
    auto findapp = _applicationlist.find(appName);
    if (findapp == _applicationlist.end()) return false;
    return findapp->second->groupCategories();
}

void Data::setApplicationGroupCategories(const QString &appName, bool value)
{
    auto findapp = _applicationlist.find(appName);
    if (findapp == _applicationlist.end()) return;
    findapp->second->setGroupCategories(value);
}

bool Data::getPaused() const
{
    return _paused;
}

void Data::setPaused(bool value)
{
    _paused = value;
}

QStringList Data::filterNames() const
{
    QStringList ret;
    for (auto filter : _filterlist) ret.append(filter.first);
    return ret;
}

void Data::checkExpiredLog()
{
    for (auto app : _applicationlist)
    {
        app.second->checkLogExpiration();
    }
}

//
// Data_Category
//

Data_Category::Data_Category(const QString &name) : _name(name), _model(), _logs(), _elapsed(), _ips() {}

LogModel *Data_Category::model()
{
    return &_model;
}

bool Data_Category::addLog(std::shared_ptr<LogModelItem> item)
{
    _logs.push_back(item);
    if (_ips.sample(1))
    {
        emit logItemsPerSecond(_name, _ips.avg());
    }
    // wait 10 items if more than 15 items/second, only show when accumulating 10 items
    if (_logs.size() > 10 || _ips.avg() < 15)
    {
        addToModel();
        return true;
    }
    return false;
}

void Data_Category::clearLog()
{
    _model.clearLogs();
    emit logAmount(_name, _model.rowCount());
}

int Data_Category::addToModel()
{
    int ret = static_cast<int>(_logs.size());
    _model.addLogs(_logs);
    _logs.clear();
    _elapsed.restart();
    emit logAmount(_name, _model.rowCount());
    return ret;
}

int Data_Category::checkLogExpiration()
{
    if (_logs.empty() || !_elapsed.hasExpired(1000)) return 0;
    int ret = addToModel();
    return ret;
}

//
// Data_Application
//

Data_Application::Data_Application(const QString &name) : _name(name), _groupCategories(false), 
    _categorylist() {}

std::shared_ptr<Data_Category> Data_Application::findCategory(const QString &categoryName)
{
    auto findcat = _categorylist.find(categoryName);
    if (findcat == _categorylist.end()) return std::shared_ptr<Data_Category>();
    return findcat->second;
}

void Data_Application::addCategory(std::shared_ptr<Data_Category> category)
{
    connect(category.get(), &Data_Category::logAmount, this, &Data_Application::categoryLogAmount);
    connect(category.get(), &Data_Category::logItemsPerSecond, this, &Data_Application::categoryLogItemsPerSecond);
    _categorylist[category->name()] = category;
}

QStringList Data_Application::categoryNames()
{
    QStringList ret;
    for (auto cat : _categorylist)
    {
        ret.append(cat.second->name());
    }
    return ret;
}

bool Data_Application::removeCategory(const QString &categoryName)
{
    return _categorylist.erase(categoryName) > 0;
}

void Data_Application::checkLogExpiration()
{
    for (auto cat : _categorylist)
    {
        cat.second->checkLogExpiration();
    }
}

void Data_Application::categoryLogAmount(const QString& categoryName, int amount)
{
    emit logAmount(_name, categoryName, amount);
}

void Data_Application::categoryLogItemsPerSecond(const QString& categoryName, double itemsPerSecond)
{
    emit logItemsPerSecond(_name, categoryName, itemsPerSecond);
}

//
// Data_Filter
//

Data_Filter::Data_Filter(const QString &name) : _name(name), _groupby(Data_Filter_GroupBy::All), 
    _filterlist() {}

bool Data_Filter::toggleFilter(const QString &appName, const QString &categoryName)
{
    QString pAppName(parseAppName(appName));
    QString find = QString("%1$$%2").arg(pAppName).arg(categoryName);
    if (_filterlist.erase(find) == 0) {
        _filterlist.insert(find);
        return true;
    }
    return false;
}

void Data_Filter::clearFilter()
{
    _filterlist.clear();
}

bool Data_Filter::isFilter(const QString &appName, const QString &categoryName)
{
    QString pAppName(parseAppName(appName));
    QString find = QString("%1$$%2").arg(pAppName).arg(categoryName);
    if (_filterlist.find(find) != _filterlist.end()) return true;
    // find with only application name
    find = QString("%1$$%2").arg(pAppName).arg("");
    return _filterlist.find(find) != _filterlist.end();
}

QString Data_Filter::filterCategoryName(const QString &appName, const QString &categoryName)
{
    QString filterCategory("ALL");
    switch (_groupby)
    {
    case Data_Filter_GroupBy::ByApplication:
        filterCategory = parseAppName(appName);
        break;
    case Data_Filter_GroupBy::ByCategory:
        filterCategory = QString("%1:%2").arg(parseAppName(appName)).arg(categoryName);
        break;
    default:
        break;
    }
    return filterCategory;
}

QString Data_Filter::parseAppName(const QString &appName)
{
    int lastColon = appName.lastIndexOf(":");
    if (lastColon == -1) return appName;
    return appName.mid(0, lastColon);
}
