#include "Data.h"

#include <QJsonArray>

Data::Data() : _applicationlist(), _groupCategories(false), _paused(false)
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
    if (_groupCategories) 
    {
        altCategory = categoryNameComplete;
        logCategory = "ALL";
    }

    internalLog(appName, time, logCategory, priority, message, source, "", altCategory);
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
            }
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

bool Data::getGroupCategories() const
{
    return _groupCategories;
}

void Data::setGroupCategories(bool value)
{
    _groupCategories = value;
}

bool Data::getPaused() const
{
    return _paused;
}

void Data::setPaused(bool value)
{
    _paused = value;
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

Data_Application::Data_Application(const QString &name) : _name(name), _categorylist() {}

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
