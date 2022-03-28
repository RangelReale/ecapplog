#include "Data.h"

Data::Data() : _applicationlist()
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

void Data::log(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    const QString &message, const QString &source)
{
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
    auto category = app->findCategory(categoryName);
    if (!category)
    {
        category = createCategory(app, categoryName);
    }

    qDebug() << appName << time.toString(Qt::ISODateWithMs) << categoryName << priority << message;
}

void Data::removeApplication(const QString &appName)
{

}

void Data::removeCategory(const QString &appName, const QString &categoryName)
{

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

//
// Data_Category
//

Data_Category::Data_Category(const QString &name) : _name(name), _model() {}

QAbstractListModel *Data_Category::model()
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
