//
// MainWindow.h
//

#pragma once

#define NOMINMAX

#include "Server.h"
#include "Data.h"

#include "DockManager.h"

#include <QListView>
#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QLabel>

#include <string>
#include <map>

class Main_Category : public QObject
{
	Q_OBJECT
public:
	Main_Category(const QString &name, QTabWidget *apptabs, QListView *logs, QLabel *logsamount) : 
		name(name), apptabs(apptabs), logs(logs), logsamount(logsamount) {}

	QString name;
	QTabWidget *apptabs;
	QListView *logs;
	QLabel *logsamount;
};

class Main_Application : public QObject
{
	Q_OBJECT
public:
	Main_Application(const QString &name, QTabWidget *categories) : name(name), categories(categories), _categorylist() {}

	QString name;
	QTabWidget *categories;

	void addCategory(std::shared_ptr<Main_Category> category);
	std::shared_ptr<Main_Category> findCategory(const QString &categoryName);
	bool removeCategory(const QString &categoryName);
private:
	typedef std::map<QString, std::shared_ptr<Main_Category> > categorylist_t;	
	categorylist_t _categorylist;
};

class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	MainWindow(QWidget *parent = 0);

    static MainWindow *instance() { return self; }
public Q_SLOTS:
	void onJsonReceived(const ApplicationInfo &appInfo, quint8 cmd, const QJsonObject &jsonData);
	void onJsonError(const ApplicationInfo& appInfo, const QJsonParseError &error);
	void onError(const QTcpSocket &clientSocket, const QString &error);

    void onNewApplication(const QString &appName);
    void onDelApplication(const QString &appName);
    void onNewCategory(const QString &appName, const QString &categoryName, QAbstractListModel *model);
    void onDelCategory(const QString &appName, const QString &categoryName);
	void onLogAmount(const QString &appName, const QString &categoryName, int amount);

	QTabWidget *createWindow();
	void logListDetail(QListView *logs);

	void menuEditClear();
	void menuEditPause();
	void menuViewGroupCategories();

	void applicationTabClose(int index);
	void applicationTabBarContextMenu(const QPoint &point);

	void categoryTabClose(int index);
	void categoryTabBarContextMenu(const QPoint &point);

	void logListContextMenu(const QPoint &point);
	void logListDoubleClicked(const QModelIndex&);
private:
	QString applicationName(const ApplicationInfo& appInfo);
	QString applicationName(const QTcpSocket &clientSocket, const QString& connname);
	QString formatJSON(const QString &json);

	void onCmdLog(const ApplicationInfo& appInfo, const QJsonObject &jsonData);

	Server _server;
	Data _data;
	int _dockCount;

	typedef std::map<QString, std::shared_ptr<Main_Application> > applicationlist_t;
	applicationlist_t _applicationlist;

	static MainWindow *self;
	ads::CDockManager* _dockManager;
	ads::CDockWidget* _rootWindow;
	QMenu *_viewMenu;
};
