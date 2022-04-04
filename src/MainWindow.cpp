#include "MainWindow.h"
#include "Config.h"
#include "LogDelegate.h"
#include "DetailWindow.h"
#include "Widgets.h"

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
#include <QClipboard>
#include <QLabel>
#include <QBoxLayout>

MainWindow *MainWindow::self;

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent), _applicationlist(), _dockCount(0), _rootWindow(nullptr)
{
	MainWindow::self = this;
	QSettings settings;

    setGeometry(0, style()->pixelMetric(QStyle::PM_TitleBarHeight), 400, 400);

	setWindowTitle("ECAppLog");
	setWindowIcon(QIcon(":/ecapplog.png"));

	// settings
	_data.setGroupCategories(settings.value("group_categories", false).toBool());

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
	connect(&_data, SIGNAL(logAmount(const QString&, const QString &, int)), this, SLOT(onLogAmount(const QString&, const QString &, int)));

	// menu: EDIT
	QMenu *editMenu = new QMenu("&Edit", this);
	
	QAction *editClear = new QAction("&Clear", this);
	connect(editClear, SIGNAL(triggered()), this, SLOT(menuEditClear()));
	editMenu->addAction(editClear);

	editMenu->addSeparator();

	QAction *editPause = new QAction("&Pause", this);
	editPause->setCheckable(true);
	connect(editPause, SIGNAL(triggered()), this, SLOT(menuEditPause()));
	editMenu->addAction(editPause);

	menuBar()->addMenu(editMenu);

	// menu: VIEW
	_viewMenu = new QMenu("&View", this);	
	QAction *viewGroupCategories = new QAction("&Group categories", this);
	viewGroupCategories->setCheckable(true);
	viewGroupCategories->setChecked(_data.getGroupCategories());
	connect(viewGroupCategories, SIGNAL(triggered()), this, SLOT(menuViewGroupCategories()));
	_viewMenu->addAction(viewGroupCategories);
	_viewMenu->addSeparator();

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

QTabWidget *MainWindow::createWindow()
{
	QTabWidget *tabs = new TabWidget;
	tabs->setTabsClosable(true);
	tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);

	connect(tabs, SIGNAL(tabCloseRequested(int)), this, SLOT(applicationTabClose(int)));
	connect(tabs->tabBar(), SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(applicationTabBarContextMenu(const QPoint &)));

	ads::CDockWidget* dockWindow = new ads::CDockWidget(QString("Window%1").arg(++_dockCount));
	if (!_rootWindow) _rootWindow = dockWindow;
	dockWindow->setWidget(tabs);
	_dockManager->addDockWidget(ads::RightDockWidgetArea, dockWindow);
	_viewMenu->addAction(dockWindow->toggleViewAction());

	return tabs;
}

void MainWindow::logListDetail(QListView *logs)
{
	QStringList slist;
	QStringList rslist;
	foreach(const QModelIndex &index,
		logs->selectionModel()->selectedIndexes()) {
		slist.append(index.data(Qt::DisplayRole).toString());
		rslist.append(formatJSON(index.data(MODELROLE_SOURCE).toString()));
	}

	DetailWindow *dwin = new DetailWindow(nullptr, slist.join("\n"), rslist.join("\n"));
	dwin->show();
}

void MainWindow::menuEditClear()
{
	_data.removeAllApplications();
	for (auto wd : _dockManager->dockWidgetsMap())
	{
		qobject_cast<QTabWidget*>(wd->widget())->clear();
	}
}

void MainWindow::menuEditPause()
{
	_data.setPaused(!_data.getPaused());
	qobject_cast<QAction*>(sender())->setChecked(_data.getPaused());
}

void MainWindow::menuViewGroupCategories()
{
	_data.setGroupCategories(!_data.getGroupCategories());

	QSettings settings;
	settings.setValue("group_categories", _data.getGroupCategories());

	qobject_cast<QAction*>(sender())->setChecked(_data.getGroupCategories());
}

void MainWindow::applicationTabClose(int index)
{
	QTabWidget *tabs = qobject_cast<QTabWidget*>(sender());
	_data.removeApplication(tabs->widget(index)->property(PROPERTY_APPNAME).toString());
	tabs->removeTab(index);
}

void MainWindow::applicationTabBarContextMenu(const QPoint &point)
{
	if (point.isNull())
		return;
 
	QTabBar *tabBar = qobject_cast<QTabBar *>(sender());
	QTabWidget *sourceTabWidget = qobject_cast<QTabWidget*>(tabBar->parentWidget());

	int tabIndex = tabBar->tabAt(point);
	QMenu menu(this);
	QAction *newWindow = menu.addAction(tr("Move to new window"));
	menu.addSeparator();
	QMenu moveTo(tr("Move to..."), this);
	for (auto key : _dockManager->dockWidgetsMap().keys())
	{
		moveTo.addAction(key);
	}
	menu.addMenu(&moveTo);

	QAction *selectedItem = menu.exec(tabBar->mapToGlobal(point));
	if (selectedItem)
	{
		ads::CDockWidget *w = nullptr;
		QTabWidget *targetTabWidget = nullptr;

		if (selectedItem == newWindow)
		{
			targetTabWidget = createWindow();
		}
		else
		{
			ads::CDockWidget *w = _dockManager->findDockWidget(selectedItem->text());
			if (w) targetTabWidget = qobject_cast<QTabWidget*>(w->widget());
		}

		if (targetTabWidget && sourceTabWidget)
		{
			targetTabWidget->addTab(sourceTabWidget->widget(tabIndex), tabBar->tabText(tabIndex));
		}
	}
}

void MainWindow::categoryTabClose(int index)
{
	QTabWidget *tabs = qobject_cast<QTabWidget*>(sender());
	_data.removeCategory(tabs->widget(index)->property(PROPERTY_APPNAME).toString(),
		tabs->widget(index)->property(PROPERTY_CATEGORYNAME).toString());
	tabs->removeTab(index);
}

void MainWindow::categoryTabBarContextMenu(const QPoint &point)
{

}

void MainWindow::logListContextMenu(const QPoint &point)
{
	QListView *list = qobject_cast<QListView*>(sender());	

	QPoint globalPos = list->mapToGlobal(point);

	QMenu myMenu;
	QAction *acopy = myMenu.addAction("&Copy to clipboard");
	QAction *acopyrs = myMenu.addAction("Copy &source to clipboard");
	QAction *aview = myMenu.addAction("&Detail...");

	QAction* selectedItem = myMenu.exec(globalPos);
	if (selectedItem)
	{
		if (selectedItem == acopy) 
		{
			QStringList slist;
			foreach(const QModelIndex &index,
				list->selectionModel()->selectedIndexes()) {
				slist.append(index.data(Qt::DisplayRole).toString());
			}
			QGuiApplication::clipboard()->setText(slist.join("\n"));
		} 
		else if (selectedItem == acopyrs) 
		{
			QStringList slist;
			foreach(const QModelIndex &index,
				list->selectionModel()->selectedIndexes()) {
				slist.append(index.data(MODELROLE_SOURCE).toString());
			}
			QGuiApplication::clipboard()->setText(slist.join("\n"));
		} 
		else if (selectedItem == aview)
		{
			logListDetail(list);
		}
	}
}

void MainWindow::logListDoubleClicked(const QModelIndex &)
{
	QListView *list = qobject_cast<QListView*>(sender());
	logListDetail(list);
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
	auto app = std::make_shared<Main_Application>(appName, new TabWidget);
	_applicationlist[app->name] = app;

	app->categories->setProperty(PROPERTY_APPNAME, appName);
	app->categories->setTabsClosable(true);
	app->categories->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);

	connect(app->categories, SIGNAL(tabCloseRequested(int)), this, SLOT(categoryTabClose(int)));
	connect(app->categories->tabBar(), SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(categoryTabBarContextMenu(const QPoint &)));

	qobject_cast<QTabWidget*>(_rootWindow->widget())->addTab(app->categories, appName);
}

void MainWindow::onDelApplication(const QString &appName)
{
	if (_applicationlist.find(appName) == _applicationlist.end()) return;
	_applicationlist.erase(appName);
}

void MainWindow::onNewCategory(const QString &appName, const QString &categoryName, QAbstractListModel *model)
{
	auto findapp = _applicationlist.find(appName);
	if (findapp == _applicationlist.end()) return;
	auto app = findapp->second;

	auto category = std::make_shared<Main_Category>(categoryName, app->categories, new QListView, new QLabel("-----"));
	app->addCategory(category);

	category->logs->setProperty(PROPERTY_APPNAME, appName);	
	category->logs->setProperty(PROPERTY_CATEGORYNAME, categoryName);	

	category->logs->setItemDelegate(new LogDelegate);
	category->logs->setSelectionMode(QAbstractItemView::ExtendedSelection);
	category->logs->setContextMenuPolicy(Qt::CustomContextMenu);

	category->logs->setModel(model);

	connect(category->logs, SIGNAL(customContextMenuRequested(const QPoint&)),
		this, SLOT(logListContextMenu(const QPoint&)));
	connect(category->logs, SIGNAL(doubleClicked(const QModelIndex&)),
		this, SLOT(logListDoubleClicked(const QModelIndex&)));

	QWidget *categoryParent = new QWidget;
	categoryParent->setProperty(PROPERTY_APPNAME, appName);	
	categoryParent->setProperty(PROPERTY_CATEGORYNAME, categoryName);	

	QVBoxLayout *layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	categoryParent->setLayout(layout);
	
	QLabel *categoryLabel = new QLabel(categoryName);
	categoryLabel->setAlignment(Qt::AlignCenter);
	categoryLabel->setStyleSheet("QLabel {padding: 4px 0;}");
	QFont font = categoryLabel->font();
	font.setPointSize(20);
	categoryLabel->setFont(font);
	QPalette palette = categoryLabel->palette();	
	palette.setColor(categoryLabel->foregroundRole(), QColor(Qt::white));
	palette.setColor(categoryLabel->backgroundRole(), QColor(Qt::darkBlue));
	// TODO: copy colors from button
	//palette.setColor(categoryLabel->foregroundRole(), qApp->style()->standardPalette().color(QPalette::Active, QPalette::ButtonText));
	//palette.setColor(categoryLabel->backgroundRole(), qApp->style()->standardPalette().color(QPalette::Active, QPalette::Button));
	categoryLabel->setPalette(palette);
	categoryLabel->setAutoFillBackground(true);

	layout->addWidget(categoryLabel);
	layout->addSpacing(6);
	layout->addWidget(category->logs);

	int idx = app->categories->addTab(categoryParent, categoryName);
	category->logsamount->setStyleSheet("QLabel{border-radius: 25px; background: red; color: white;}");
	category->logsamount->setAlignment(Qt::AlignCenter);
	app->categories->tabBar()->setTabButton(idx, QTabBar::RightSide, category->logsamount);
}

void MainWindow::onDelCategory(const QString &appName, const QString &categoryName)
{
	auto app = _applicationlist.find(appName);
	if (app == _applicationlist.end()) return;	
	app->second->removeCategory(categoryName);
}

void MainWindow::onLogAmount(const QString &appName, const QString &categoryName, int amount)
{
	auto app = _applicationlist.find(appName);
	if (app == _applicationlist.end()) return;	

	auto category = app->second->findCategory(categoryName);
	if (!category) return;

	category->logsamount->setText(QString("%1").arg(amount));
}

QString MainWindow::formatJSON(const QString &json)
{
	QJsonDocument doc = QJsonDocument::fromJson(json.toLocal8Bit());
	if (doc.isNull()) return json;
	return doc.toJson(QJsonDocument::Indented);
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

bool Main_Application::removeCategory(const QString &categoryName)
{
	return _categorylist.erase(categoryName) > 0;
}

