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
#include <QActionGroup>
#include <QInputDialog>
#include <QDateTime>
#include <QSettings>
#include <QVariant>
#include <QMetaType>
#include <QClipboard>
#include <QLabel>
#include <QBoxLayout>
#include <QListIterator>
#ifdef ECAPPLOG_DEBUG_MENUS
#include <QRandomGenerator>
#endif

#define FILTERMENU_FILTERNAME       	"ECL_FILTERNAME"
#define FILTERMENU_GROUPBY       	    "ECL_FILTERGROUPBY"


MainWindow *MainWindow::self;

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent), _applicationlist(), _dockCount(0), _rootWindow(nullptr),
	_data(), _server()
{
	MainWindow::self = this;
	QSettings settings;

    setGeometry(0, style()->pixelMetric(QStyle::PM_TitleBarHeight), 400, 400);

	refreshWindowTitle();
	setWindowIcon(QIcon(":/ecapplog"));

	// settings
	_data.setGroupCategories(settings.value("group_categories", false).toBool());

	_dockManager = new ads::CDockManager(this);

	// server
	connect(&_server, &Server::onJsonReceived, this, &MainWindow::onJsonReceived);
	connect(&_server, &Server::onJsonError, this, &MainWindow::onJsonError);
	connect(&_server, &Server::onError, this, &MainWindow::onError);

	if (!_server.startServer()) {
		QMessageBox::critical(this, tr("Server"),
			tr("Unable to start the server: %1.")
			.arg(_server.errorString()));
		close();
		return;
	}

	// data
	connect(&_data, &Data::newApplication, this, &MainWindow::onNewApplication);
	connect(&_data, &Data::delApplication, this, &MainWindow::onDelApplication);
	connect(&_data, &Data::newCategory, this, &MainWindow::onNewCategory);
	connect(&_data, &Data::delCategory, this, &MainWindow::onDelCategory);
	connect(&_data, &Data::logAmount, this, &MainWindow::onLogAmount);
	connect(&_data, &Data::logItemsPerSecond, this, &MainWindow::onLogItemsPerSecond);
	connect(&_data, &Data::newFilter, this, &MainWindow::onNewFilter);
	connect(&_data, &Data::filterChanged, this, &MainWindow::onFilterChanged);

	// menu: EDIT
	QMenu *editMenu = new QMenu("&Edit", this);
	editMenu->addAction("&Clear", this, &MainWindow::menuEditClear);
	editMenu->addSeparator();
	QAction *editPause = editMenu->addAction("&Pause", this, &MainWindow::menuEditPause);
	editPause->setCheckable(true);

	menuBar()->addMenu(editMenu);

	// menu: VIEW
	_viewMenu = new QMenu("&View", this);	
	QAction *viewGroupCategories = _viewMenu->addAction("&Group categories", this, &MainWindow::menuViewGroupCategories);
	viewGroupCategories->setCheckable(true);
	viewGroupCategories->setChecked(_data.getGroupCategories());
	_viewMenu->addSeparator();
	_viewMenu->addAction("&New window", this, &MainWindow::menuViewNewWindow);
	_viewMenu->addSeparator();

	menuBar()->addMenu(_viewMenu);

	// menu: FILTER
	_filterMenu = new QMenu("&Filter", this);
	_filterMenu->addAction("&New filter", this, &MainWindow::menuFilterNew);
	_filterMenu->addSeparator();

	menuBar()->addMenu(_filterMenu);

#ifdef ECAPPLOG_DEBUG_MENUS
	// menu: DEBUG
	QMenu *debugMenu = new QMenu("&Debug", this);
	debugMenu->addAction("&Publish logs", this, &MainWindow::menuDebugPublishLogs);

	menuBar()->addMenu(debugMenu);
#endif

	// initialization
	createWindow();
	menuFilterNew();
}

QTabWidget *MainWindow::createWindow()
{
	QTabWidget *tabs = new TabWidget;
	tabs->setTabsClosable(true);
	tabs->setMovable(true);
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

	QListIterator<QModelIndex> iter(logs->selectionModel()->selectedIndexes());
	iter.toBack();
	while (iter.hasPrevious()) {
		auto index(iter.previous());
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
	refreshWindowTitle();
}

void MainWindow::menuViewGroupCategories()
{
	_data.setGroupCategories(!_data.getGroupCategories());

	QSettings settings;
	settings.setValue("group_categories", _data.getGroupCategories());

	qobject_cast<QAction*>(sender())->setChecked(_data.getGroupCategories());
}

void MainWindow::menuViewNewWindow()
{
	createWindow();
}

void MainWindow::menuFilterNew()
{
	_data.insertFilter();
}

void MainWindow::menuFilterClear()
{
	QAction *action = qobject_cast<QAction*>(sender());
	if (!action) return;
	QVariant filterName = action->property(FILTERMENU_FILTERNAME);
	if (!filterName.isValid()) return;
	_data.clearFilter(filterName.toString());
}

void MainWindow::menuFilterGroupBy()
{
	QAction *action = qobject_cast<QAction*>(sender());
	if (!action) return;
	QVariant filterName = action->property(FILTERMENU_FILTERNAME);
	QVariant groupBy = action->property(FILTERMENU_GROUPBY);
	if (!filterName.isValid()) return;
	_data.setFilterGroupBy(filterName.toString(), static_cast<Data_Filter_GroupBy>(groupBy.toInt()));
}

void MainWindow::refreshWindowTitle()
{
	QString title("ECAppLog");
	if (_data.getPaused()) title.append(" [paused]");
	setWindowTitle(title);
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

	QString appName(sourceTabWidget->widget(tabIndex)->property(PROPERTY_APPNAME).toString());

	QMenu filterMenu(tr("Toggle application on filter"), this);
	if (!appName.startsWith("FILTER")) 
	{
		for (auto filter : _data.filterNames())
		{
			QAction *filterItem = filterMenu.addAction(filter);
			filterItem->setProperty("ECL_FILTER", true);
		}
		menu.addMenu(&filterMenu);
		menu.addSeparator();
	}

	QAction* clearCategory = menu.addAction("&Clear categories");
	QAction* clearCategoryLogs = menu.addAction("C&lear all logs from all categories");
	bool appGroupCategories = _data.getApplicationGroupCategories(appName);
	QAction *groupCategories = menu.addAction("&Group categories");
	groupCategories->setCheckable(true);
	groupCategories->setChecked(appGroupCategories);

	menu.addSeparator();
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
		if (selectedItem == groupCategories)
		{
			_data.setApplicationGroupCategories(appName, !appGroupCategories);
			return;
		}
		else if (selectedItem == clearCategory)
		{
			_data.clearApplication(appName);
			return;
		}
		else if (selectedItem == clearCategoryLogs)
		{
			_data.clearApplicationLogs(appName);
			return;
		}
		if (selectedItem->property("ECL_FILTER").isValid())
		{
			QString filterName = selectedItem->text();
			_data.toggleFilter(filterName, appName, "");
			return;
		}

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
}

void MainWindow::categoryTabBarContextMenu(const QPoint &point)
{
	if (point.isNull())
		return;
 
	QTabBar *tabBar = qobject_cast<QTabBar *>(sender());
	QTabWidget *sourceTabWidget = qobject_cast<QTabWidget*>(tabBar->parentWidget());

	int tabIndex = tabBar->tabAt(point);
	QMenu menu(this);

	QString appName(sourceTabWidget->widget(tabIndex)->property(PROPERTY_APPNAME).toString());
	QString categoryName(sourceTabWidget->widget(tabIndex)->property(PROPERTY_CATEGORYNAME).toString());

	QAction* moveToFront = menu.addAction("Move category to &front");
	QAction* moveToBack = menu.addAction("Move category to &back");
	menu.addSeparator();
	QAction* clearCategory = menu.addAction("&Clear");

	QMenu filterMenu(tr("Toggle category on filter"), this);
	if (!appName.startsWith("FILTER"))
	{
		menu.addSeparator();
		
		for (auto filter : _data.filterNames())
		{
			QAction* filterItem = filterMenu.addAction(filter);
			filterItem->setProperty("ECL_FILTER", true);
		}
		menu.addMenu(&filterMenu);
	}

	QAction *selectedItem = menu.exec(tabBar->mapToGlobal(point));
	if (selectedItem)
	{
		if (selectedItem == moveToFront)
		{
			sourceTabWidget->tabBar()->moveTab(tabIndex, 0);

			/*
			bool isCurrent = sourceTabWidget->currentIndex() == tabIndex;
			sourceTabWidget->insertTab(0, sourceTabWidget->widget(tabIndex), tabBar->tabText(tabIndex));
			if (isCurrent) sourceTabWidget->setCurrentIndex(0);
			*/
		}
		else if (selectedItem == moveToBack)
		{
			sourceTabWidget->tabBar()->moveTab(tabIndex, sourceTabWidget->count() - 1);

			/*
			bool isCurrent = sourceTabWidget->currentIndex() == tabIndex;
			sourceTabWidget->addTab(sourceTabWidget->widget(tabIndex), tabBar->tabText(tabIndex));
			if (isCurrent) sourceTabWidget->setCurrentIndex(sourceTabWidget->count() - 1);
			*/
		}
		else if (selectedItem == clearCategory)
		{
			_data.clearCategory(appName, categoryName);
		}
		else if (selectedItem->property("ECL_FILTER").isValid())
		{
			QString filterName = selectedItem->text();
			_data.toggleFilter(filterName, appName, categoryName);
		}
	}
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
			QListIterator<QModelIndex> iter(list->selectionModel()->selectedIndexes());
			iter.toBack();
			while (iter.hasPrevious()) {
				slist.append(iter.previous().data(Qt::DisplayRole).toString());
			}
			QGuiApplication::clipboard()->setText(slist.join("\n"));
		} 
		else if (selectedItem == acopyrs) 
		{
			QStringList slist;
			QListIterator<QModelIndex> iter(list->selectionModel()->selectedIndexes());
			iter.toBack();
			while (iter.hasPrevious()) {
				slist.append(iter.previous().data(MODELROLE_SOURCE).toString());
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

void MainWindow::onJsonReceived(const QString& appName, quint8 cmd, const QJsonObject &jsonData)
{
	switch (cmd)
	{
	case CMD_LOG:
		_data.log(appName, jsonData);
		break;
	default:
		_data.log(appName, QDateTime(), "ECAPPLOG", Priority::PRIO_ERROR, 
			QString("Unknown command: %1").arg(cmd));
	}
}

void MainWindow::onJsonError(const QString& appName, const QJsonParseError &error)
{
	_data.log(appName, QDateTime(), "ECAPPLOG", Priority::PRIO_ERROR, 
		QString("JSON parse error: %1").arg(error.errorString()));
}

void MainWindow::onError(const QString& appName, const QString &error)
{
	_data.log(appName, QDateTime(), "ECAPPLOG", Priority::PRIO_ERROR, 
		QString("Error: %1").arg(error));
}

void MainWindow::onCmdLog(const QString& appName, const QJsonObject &jsonData)
{
	_data.log(appName, jsonData);
}

void MainWindow::onNewApplication(const QString &appName)
{
	auto app = std::make_shared<Main_Application>(appName, new TabWidget);
	_applicationlist[app->name] = app;

	app->categories->setProperty(PROPERTY_APPNAME, appName);
	app->categories->setTabsClosable(true);
	app->categories->setMovable(true);
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

	int idx;
	if (categoryName.isUpper()) {
		// put all-uppercase categories in front
		idx = app->categories->insertTab(0, categoryParent, categoryName);
	} else {
		idx = app->categories->addTab(categoryParent, categoryName);
	}
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

void MainWindow::onLogItemsPerSecond(const QString& appName, const QString& categoryName, double itemsPerSecond)
{
	/*
	auto app = _applicationlist.find(appName);
	if (app == _applicationlist.end()) return;

	auto category = app->second->findCategory(categoryName);
	if (!category) return;

	category->logsamount->setText(QString("%1").arg(itemsPerSecond, 0, 'f', 1));
	*/
}

void MainWindow::onNewFilter(const QString &filterName)
{
	QMenu *newFilter = new QMenu(filterName, this);
	newFilter->setProperty(FILTERMENU_FILTERNAME, filterName);
	_filterMenu->addMenu(newFilter);

	QAction *fClear = new QAction("Clear", newFilter);
	fClear->setProperty(FILTERMENU_FILTERNAME, filterName);
	connect(fClear, SIGNAL(triggered()), this, SLOT(menuFilterClear()));
	newFilter->addAction(fClear);

	newFilter->addSeparator();

	QActionGroup *filterGroupBy = new QActionGroup(this);

	QAction *fGroupBy = new QAction("&Single category", this);
	fGroupBy->setProperty(FILTERMENU_FILTERNAME, filterName);
	fGroupBy->setProperty(FILTERMENU_GROUPBY, static_cast<int>(Data_Filter_GroupBy::All));
	connect(fGroupBy, SIGNAL(triggered()), this, SLOT(menuFilterGroupBy()));
	fGroupBy->setCheckable(true);
	fGroupBy->setChecked(true);
	filterGroupBy->addAction(fGroupBy);
	newFilter->addAction(fGroupBy);

	fGroupBy = new QAction("Groub by &category", this);
	fGroupBy->setProperty(FILTERMENU_FILTERNAME, filterName);
	fGroupBy->setProperty(FILTERMENU_GROUPBY, static_cast<int>(Data_Filter_GroupBy::ByCategory));
	connect(fGroupBy, SIGNAL(triggered()), this, SLOT(menuFilterGroupBy()));
	fGroupBy->setCheckable(true);
	filterGroupBy->addAction(fGroupBy);
	newFilter->addAction(fGroupBy);

	fGroupBy = new QAction("Group by &application", this);
	fGroupBy->setProperty(FILTERMENU_FILTERNAME, filterName);
	fGroupBy->setProperty(FILTERMENU_GROUPBY, static_cast<int>(Data_Filter_GroupBy::ByApplication));
	connect(fGroupBy, SIGNAL(triggered()), this, SLOT(menuFilterGroupBy()));
	fGroupBy->setCheckable(true);
	filterGroupBy->addAction(fGroupBy);
	newFilter->addAction(fGroupBy);

}

void MainWindow::onFilterChanged(const QString &filterName)
{

}

QString MainWindow::formatJSON(const QString &json)
{
	QJsonDocument doc = QJsonDocument::fromJson(json.toLocal8Bit());
	if (doc.isNull()) return json;
	return doc.toJson(QJsonDocument::Indented);
}

#ifdef ECAPPLOG_DEBUG_MENUS
void MainWindow::menuDebugPublishLogs()
{
	QString appName(QString("DEBUG%1").arg(QRandomGenerator::global()->generate() % 100));

	for (int ct=0; ct<100; ct++)
		_data.log(appName, QDateTime::currentDateTimeUtc(), "debug", Priority::PRIO_DEBUG, QString("Debug message %1").arg(ct));
	for (int ct=0; ct<98; ct++)
		_data.log(appName, QDateTime::currentDateTimeUtc(), "info", Priority::PRIO_INFORMATION, QString("Info message %1").arg(ct));
	for (int ct=0; ct<50; ct++)
		_data.log(appName, QDateTime::currentDateTimeUtc(), "error", Priority::PRIO_ERROR, QString("Error message %1").arg(ct));
}
#endif

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
	bool ret = _categorylist.erase(categoryName) > 0;
	if (ret)
	{
		for (int i=0; i < categories->count(); i++)
		{
			if (categories->widget(i)->property(PROPERTY_CATEGORYNAME).toString() == categoryName)
			{
				categories->removeTab(i);
				break;
			}			
		}
	}
	return ret;
}
