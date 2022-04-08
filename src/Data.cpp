#include "Data.h"

#include <QJsonArray>

Data::Data() : _applicationlist(), _filterlist(), _filtercount(0), _groupCategories(false), _paused(false)
{

}

void Data::log(const QString &appName, const QJsonObject &jsonData)
{
	QString f_category, f_time, f_priority, f_message, f_source, f_originalCategory;
    QStringList f_extraCategories;

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

    log(appName, time, f_category, f_priority, f_message, f_source, f_originalCategory, f_extraCategories);
}

void Data::log(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    const QString &message, const QString &source, const QString &originalCategory, const QStringList &extraCategories)
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

    internalLog(appName, time, logCategory, priority, message, source, "", altCategory);
    logFilter(appName, time, logCategory, priority, message, source);
    if (Priority::isErrorOrWarning(priority))
    {
        internalLog(appName, time, "ERROR", priority, message, source, "", categoryNameComplete);
    }

    if (!extraCategories.isEmpty())
    {
        for ( const auto& extraCategory : extraCategories  )
        {
            if (extraCategory != logCategory) {
                internalLog(appName, time, extraCategory, priority, message, source, "", categoryNameComplete, true);
                logFilter(appName, time, extraCategory, priority, message, source);
            }
        }
    }
}

void Data::logFilter(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    const QString &message, const QString &source)
{
    for (auto filter : _filterlist)
    {
        if (filter.second->isFilter(appName, categoryName))
        {
            internalLog(filter.second->name(), time, filter.second->filterCategoryName(appName, categoryName), 
                priority, message, source, appName, categoryName);
        }
    }
}

void Data::internalLog(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    const QString &message, const QString &source, const QString &altApp, const QString &altCategory, bool isExtraCategory)
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

    addToModel(category->model(), appName, time, appCategory, priority, message, source, altApp, altCategory, isExtraCategory);
}


void Data::addToModel(LogModel *model, const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    const QString &message, const QString &source, const QString &altApp, const QString &altCategory, bool isExtraCategory)
{
    model->addLog(appName, time, categoryName, priority, message, source, altApp, altCategory, isExtraCategory);
    emit logAmount(appName, categoryName, model->rowCount(QModelIndex()));
}

void Data::removeApplication(const QString &appName)
{
    if (_applicationlist.find(appName) == _applicationlist.end()) return;
    _applicationlist.erase(appName);
    emit delApplication(appName);
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

void Data::toogleFilter(const QString &filterName, const QString &appName, const QString &categoryName)
{
    auto findfilter = _filterlist.find(filterName);
    if (findfilter == _filterlist.end()) return;

    findfilter->second->toogleFilter(appName, categoryName);
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

//
// Data_Category
//

Data_Category::Data_Category(const QString &name) : _name(name), _model() {}

LogModel *Data_Category::model()
{
    return &_model;
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
    _categorylist[category->name()] = category;
}

bool Data_Application::removeCategory(const QString &categoryName)
{
    return _categorylist.erase(categoryName) > 0;
}

//
// Data_Filter
//

Data_Filter::Data_Filter(const QString &name) : _name(name), _groupby(Data_Filter_GroupBy::All), 
    _filterlist() {}

void Data_Filter::toogleFilter(const QString &appName, const QString &categoryName)
{
    QString pAppName(parseAppName(appName));
    QString find = QString("%1$$%2").arg(pAppName).arg(categoryName);
    if (_filterlist.erase(find) == 0) _filterlist.insert(find);
}

void Data_Filter::clearFilter()
{
    _filterlist.clear();
}

bool Data_Filter::isFilter(const QString &appName, const QString &categoryName)
{
    QString pAppName(parseAppName(appName));
    QString find = QString("%1$$%2").arg(pAppName).arg(categoryName);
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
