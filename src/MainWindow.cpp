#include "MainWindow.h"
#include "Config.h"
#include "LogDelegate.h"

#include <QApplication>
#include <QStyle>
#include <QMenu>
#include <QMessageBox>
#include <QMenuBar>
#include <QAction>
#include <QInputDialog>
#include <QDateTime>
#include <QSettings>
#include <QVariant>
#include <QMetaType>

MainWindow *MainWindow::self;

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent), _applicationlist(), _dockCount(0), _rootWindow(nullptr)
{
	MainWindow::self = this;

    setGeometry(0, style()->pixelMetric(QStyle::PM_TitleBarHeight), 400, 400);

	setWindowTitle("ECAppLog");
	setWindowIcon(QIcon(":/ecapplog.png"));

	_dockManager = new ads::CDockManager(this);

	// server
	connect(&_server, SIGNAL(onJsonReceived(const ApplicationInfo&, quint8, const QJsonObject&)), this, SLOT(onJsonReceived(const ApplicationInfo&, quint8, const QJsonObject&)));
	connect(&_server, SIGNAL(onJsonError(const ApplicationInfo&, const QJsonParseError&)), this, SLOT(onJsonError(const ApplicationInfo&, const QJsonParseError&)));
	connect(&_server, SIGNAL(onError(const QTcpSocket&, const QString&)), this, SLOT(onError(const QTcpSocket&, const QString&)));

	if (!_server.startServer()) {
		QMessageBox::critical(this, tr("Server"),
			tr("Unable to start the server: %1.")
			.arg(_server.errorString()));
		close();
		return;
	}

	// data
	connect(&_data, SIGNAL(newApplication(const QString&)), this, SLOT(onNewApplication(const QString&)));
	connect(&_data, SIGNAL(delApplication(const QString&)), this, SLOT(onDelApplication(const QString&)));
	connect(&_data, SIGNAL(newCategory(const QString&, const QString &, QAbstractListModel *)), this, SLOT(onNewCategory(const QString&, const QString &, QAbstractListModel *)));
	connect(&_data, SIGNAL(delCategory(const QString&, const QString &)), this, SLOT(onDelCategory(const QString&, const QString &)));

	// menu: VIEW
	_viewMenu = new QMenu("&View", this);
	menuBar()->addMenu(_viewMenu);

	// initialization
	createWindow();
}

QString MainWindow::applicationName(const ApplicationInfo& appInfo)
{
	return applicationName(*appInfo.socket(), appInfo.getName());
}

QString MainWindow::applicationName(const QTcpSocket &clientSocket, const QString& connname)
{
	if (connname.isEmpty())
		return QString("%1:%2").arg(clientSocket.peerAddress().toString()).arg(clientSocket.peerPort());
	return connname;
}

void MainWindow::createWindow()
{
	QTabWidget *tabs = new QTabWidget;
	tabs->setTabsClosable(true);

	ads::CDockWidget* dockWindow = new ads::CDockWidget(QString("Window%1").arg(++_dockCount));
	if (!_rootWindow) _rootWindow = dockWindow;
	dockWindow->setWidget(tabs);
	_dockManager->addDockWidget(ads::RightDockWidgetArea, dockWindow);
	_viewMenu->addAction(dockWindow->toggleViewAction());
}

void MainWindow::onJsonReceived(const ApplicationInfo& appInfo, quint8 cmd, const QJsonObject &jsonData)
{
	switch (cmd)
	{
	case CMD_LOG:
		_data.log(applicationName(appInfo), jsonData);
		break;
	default:
		_data.log(applicationName(appInfo), QDateTime(), "ECAPPLOG", Priority::PRIO_ERROR, 
			QString("Unknown command: %1").arg(cmd));
	}
}

void MainWindow::onJsonError(const ApplicationInfo& appInfo, const QJsonParseError &error)
{
	_data.log(applicationName(appInfo), QDateTime(), "ECAPPLOG", Priority::PRIO_ERROR, 
		QString("JSON parse error: %1").arg(error.errorString()));
}

void MainWindow::onError(const QTcpSocket &clientSocket, const QString &error)
{
	_data.log(applicationName(clientSocket, ""), QDateTime(), "ECAPPLOG", Priority::PRIO_ERROR, 
		QString("Error: %1").arg(error));
}

void MainWindow::onCmdLog(const ApplicationInfo& appInfo, const QJsonObject &jsonData)
{
	_data.log(applicationName(appInfo), jsonData);
}

void MainWindow::onNewApplication(const QString &appName)
{
	qDebug() << "newApplication" << appName;

	auto app = std::make_shared<Main_Application>(appName, new QTabWidget);
	_applicationlist[app->name] = app;

	app->categories->setProperty(PROPERTY_APPNAME, appName);
	app->categories->setTabsClosable(true);

	qobject_cast<QTabWidget*>(_rootWindow->widget())->addTab(app->categories, appName);
}

void MainWindow::onDelApplication(const QString &appName)
{

}

void MainWindow::onNewCategory(const QString &appName, const QString &categoryName, QAbstractListModel *model)
{
	qDebug() << "newCategory" << appName << categoryName;

	auto findapp = _applicationlist.find(appName);
	if (findapp == _applicationlist.end()) return;
	auto app = findapp->second;

	auto category = std::make_shared<Main_Category>(categoryName, new QListView);
	app->addCategory(category);

	category->logs->setProperty(PROPERTY_APPNAME, appName);	
	category->logs->setProperty(PROPERTY_CATEGORYNAME, categoryName);	

	category->logs->setItemDelegate(new LogDelegate);
	category->logs->setSelectionMode(QAbstractItemView::ExtendedSelection);
	//category->logs->setContextMenuPolicy(Qt::CustomContextMenu);

	category->logs->setModel(model);

	app->categories->addTab(category->logs, categoryName);
}

void MainWindow::onDelCategory(const QString &appName, const QString &categoryName)
{

}

//
// Main_Application
//

void Main_Application::addCategory(std::shared_ptr<Main_Category> category)
{
	_categorylist[category->name] = category;
}

std::shared_ptr<Main_Category> Main_Application::findCategory(const QString &categoryName)
{
	auto find = _categorylist.find(categoryName);
	if (find == _categorylist.end()) return std::shared_ptr<Main_Category>();
	return find->second;
}
