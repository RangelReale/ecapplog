#include "MainWindow.h"
#include "Config.h"
#include "LogDelegate.h"
#include "DetailWindow.h"
#include "FindDialog.h"
#include "Widgets.h"

#include <QApplication>
#include <QStyle>
#include <QMenu>
#include <QMessageBox>
#include <QMenuBar>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QInputDialog>
#include <QLineEdit>
#include <QDateTime>
#include <QSettings>
#include <QVariant>
#include <QMetaType>
#include <QClipboard>
#include <QLabel>
#include <QBoxLayout>
#include <QListIterator>
#include <QTimer>
#include <QEvent>
#include <QFontInfo>
#ifdef ECAPPLOG_DEBUG_MENUS
#include <QRandomGenerator>
#endif

#include <algorithm>

#define FILTERMENU_FILTERNAME       	"ECL_FILTERNAME"
#define FILTERMENU_GROUPBY       	    "ECL_FILTERGROUPBY"

// Font size bounds offered by View -> Font size, and how much larger a category header is drawn
// than the log text below it. The lower bound has to stay below the smallest platform font size we
// might adopt as a default (Windows uses 9pt), otherwise defaultFontSize() gets clamped upwards.
#define MIN_FONT_SIZE			8
#define MAX_FONT_SIZE			98
#define MAC_DEFAULT_FONT_SIZE	16
#define HEADER_FONT_OFFSET		4

namespace {

// macOS is the only platform whose font we override: the system font reads small next to the Fusion
// style substituted in main.cpp. Everywhere else the platform font is the right starting point.
int defaultFontSize()
{
#ifdef Q_OS_DARWIN
	return MAC_DEFAULT_FONT_SIZE;
#else
	// QFontInfo resolves the size actually in use, unlike QFont::pointSize(), which returns -1 when
	// the platform font is specified in pixels (see the note in menuViewFont).
	return QFontInfo(qApp->font()).pointSize();
#endif
}

// Binds every key sequence the platform defines for a standard action, not just the first one:
// Qt lists F3 ahead of Cmd+G for FindNext, so setShortcut() alone would leave a Mac without the
// shortcut it expects. The menu label shows the first sequence, hence the reordering.
void setStandardShortcuts(QAction *action, QKeySequence::StandardKey key)
{
	QList<QKeySequence> shortcuts = QKeySequence::keyBindings(key);
#ifdef Q_OS_DARWIN
	std::stable_partition(shortcuts.begin(), shortcuts.end(), [](const QKeySequence &shortcut) {
		// Qt::ControlModifier is the Command key here, so this picks the native sequence
		return shortcut.count() > 0 && (shortcut[0] & Qt::ControlModifier);
	});
#endif
	action->setShortcuts(shortcuts);
	// Windows can be floated out of the main window into top level windows of their own, where the
	// default WindowShortcut context would stop the shortcuts from firing.
	action->setShortcutContext(Qt::ApplicationShortcut);
}

// Same application wide context as setStandardShortcuts, for the actions Qt has no standard key
// for. Ctrl is the Command key on macOS, so one sequence covers every platform.
void setShortcut(QAction *action, const QKeySequence &sequence)
{
	action->setShortcut(sequence);
	action->setShortcutContext(Qt::ApplicationShortcut);
}

}


MainWindow *MainWindow::self;

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent), _applicationlist(), _dockCount(0), _fontSize(defaultFontSize()),
	_rootWindow(nullptr), _data(), _server(), _findText(), _findCaseSensitive(false),
	_lastLogView(), _findHighlightView(), _findHighlightIndex(), _findHighlightShown(false),
	_highlight(), _highlightMenu(nullptr)
{
	MainWindow::self = this;
	QSettings settings;

    setGeometry(0, style()->pixelMetric(QStyle::PM_TitleBarHeight), 400, 400);

	refreshWindowTitle();
	setWindowIcon(QIcon(":/ecapplog"));

	// settings
	_data.setGroupCategories(settings.value("group_categories", false).toBool());
	// clamped to the range menuViewFont offers, so a hand-edited value cannot produce an
	// unusable window
	_fontSize = qBound(MIN_FONT_SIZE, settings.value("font_size", defaultFontSize()).toInt(),
		MAX_FONT_SIZE);

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

	// The menu actions have no sender to recover a log view from, the way the context menu and
	// double-click handlers do, so remember the last log view that had the focus. The find dialog
	// itself never clobbers this: a QLineEdit is not a QListView.
	connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
		if (QListView *view = qobject_cast<QListView*>(now)) _lastLogView = view;
		// A search result is only highlighted for as long as that view is the one being used, so
		// moving to another tab, window or dialog takes the highlight back with it. Two cases are
		// deliberately left alone: the focus leaving the application altogether, and popups such
		// as the log context menu, whose Copy and Detail entries act on the selected row.
		if (now && now != _findHighlightView && !(now->window()->windowFlags() & Qt::Popup))
		{
			clearFindHighlight();
		}
	});

	// menu: EDIT
	QMenu *editMenu = new QMenu("&Edit", this);
	editMenu->addAction("&Clear", this, &MainWindow::menuEditClear);
	editMenu->addSeparator();
	QAction *editPause = editMenu->addAction("&Pause", this, &MainWindow::menuEditPause);
	editPause->setCheckable(true);
	editMenu->addSeparator();
	setStandardShortcuts(editMenu->addAction("&Find...", this, &MainWindow::menuEditFind),
		QKeySequence::Find);
	setStandardShortcuts(editMenu->addAction("Find &next", this, &MainWindow::menuEditFindNext),
		QKeySequence::FindNext);
	setStandardShortcuts(editMenu->addAction("Find &previous", this, &MainWindow::menuEditFindPrevious),
		QKeySequence::FindPrevious);

	menuBar()->addMenu(editMenu);

	// menu: VIEW
	_viewMenu = new QMenu("&View", this);
	QAction *viewGroupCategories = _viewMenu->addAction("&Group categories", this, &MainWindow::menuViewGroupCategories);
	viewGroupCategories->setCheckable(true);
	viewGroupCategories->setChecked(_data.getGroupCategories());
	// Above the separator: createWindow appends the dock toggles after it, and they are meant to
	// stay a group of their own.
	_highlightMenu = new QMenu("&Highlight", this);
	_viewMenu->addMenu(_highlightMenu);
	refreshHighlightMenu();
	_viewMenu->addSeparator();
	_viewMenu->addAction("&New window", this, &MainWindow::menuViewNewWindow);
    _viewMenu->addAction("&Font size", this, &MainWindow::menuViewFont);
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

	applyFontSize(_fontSize);
	// ...and again once the event loop is running. Widgets that resolve their font lazily (the
	// macOS theme supplies per-widget-class fonts) would otherwise keep the system size until the
	// user opened the dialog, which is exactly the startup bug this fixes.
	QTimer::singleShot(0, this, [this] { applyFontSize(_fontSize); });
}

void MainWindow::applyFontSize(int pointSize)
{
	_fontSize = pointSize;

	QFont font = qApp->font();
	font.setPointSize(pointSize);
	qApp->setFont(font);	// so widgets created later inherit the size

	// Propagate to existing widgets explicitly instead of relying on setFont to do it:
	// QApplication only notifies them when it has a per-widget-class font hash to clear, which is
	// why the first size change worked and every later one appeared to do nothing.
	QEvent fontChange(QEvent::ApplicationFontChange);
	for (QWidget *widget : qApp->allWidgets())
	{
		qApp->sendEvent(widget, &fontChange);
	}

	// Category headers stay deliberately larger than the log text.
	QFont headerFont(font);
	headerFont.setPointSize(pointSize + HEADER_FONT_OFFSET);

	for (auto &entry : _applicationlist)
	{
		entry.second->applyFont(headerFont);
	}
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

// The log view a menu-triggered action should act on: the one the user last clicked into, falling
// back to whatever the root window currently shows if no log view has ever had the focus.
QListView *MainWindow::currentLogView() const
{
	// isVisible() rejects a view whose category or application tab is no longer the selected one
	if (_lastLogView && _lastLogView->isVisible()) return _lastLogView;

	if (!_rootWindow) return nullptr;
	QTabWidget *applications = qobject_cast<QTabWidget*>(_rootWindow->widget());
	if (!applications) return nullptr;
	QTabWidget *categories = qobject_cast<QTabWidget*>(applications->currentWidget());
	if (!categories) return nullptr;
	QWidget *categoryParent = categories->currentWidget();
	if (!categoryParent) return nullptr;

	return categoryParent->findChild<QListView*>();
}

// Searches the message text of every row exactly once, starting after (or before, when going
// backwards) the current one and wrapping around, so a failure means there really is no match.
bool MainWindow::findInLogView(QListView *logs, bool backwards)
{
	if (!logs || _findText.isEmpty()) return false;

	QAbstractItemModel *model = logs->model();
	if (!model) return false;

	int rows = model->rowCount();
	if (rows < 1) return false;

	// Logs keep arriving while a search is in progress, pushing every row down (LogModel inserts
	// at row 0). Both indexes below are persistent, so the anchor follows its own item.
	QModelIndex current = logs->currentIndex();
	if (!current.isValid() && _findHighlightView == logs && _findHighlightIndex.isValid()
		&& _findHighlightIndex.model() == model)
	{
		// the previous match here had its highlight taken back, but it is still where to resume from
		current = _findHighlightIndex;
	}
	int start = current.isValid() ? current.row() : (backwards ? rows : -1);

	Qt::CaseSensitivity caseSensitivity = _findCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
	int step = backwards ? -1 : 1;

	for (int offset = 1; offset <= rows; ++offset)
	{
		int row = ((start + (offset * step)) % rows + rows) % rows;
		QModelIndex index = model->index(row, 0);
		if (index.data(MODELROLE_MESSAGE).toString().contains(_findText, caseSensitivity))
		{
			clearFindHighlight();
			// the focus keeps the highlight from being taken back straight away, and lets the
			// repeat shortcuts and the arrow keys carry on from the match
			logs->setFocus();
			// Deliberately not QAbstractItemView::setCurrentIndex, which asks selectionCommand()
			// what to do and that reads the modifiers being held right now: the Command in Cmd+G
			// is Qt::ControlModifier, so it would answer Toggle and pile every match onto the
			// selection instead of replacing it (Shift+Cmd+G would extend a range).
			logs->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
			logs->scrollTo(index, QAbstractItemView::PositionAtCenter);
			_findHighlightView = logs;
			_findHighlightIndex = index;
			_findHighlightShown = true;
			// watched so that switching away from the tab takes the highlight back too, which
			// does not always go through the focus change above
			logs->installEventFilter(this);
			return true;
		}
	}

	return false;
}

// Takes back both marks a match leaves behind: the selection, and the current index, which the
// delegate paints as a pale band of its own and which would otherwise sit there indefinitely.
// Anything the user has selected or made current since is theirs, and is left alone.
void MainWindow::clearFindHighlight()
{
	if (!_findHighlightShown) return;
	_findHighlightShown = false;

	QListView *view = _findHighlightView;
	if (!view) return;
	view->removeEventFilter(this);

	QItemSelectionModel *selection = view->selectionModel();
	if (!selection || !_findHighlightIndex.isValid()) return;

	QModelIndex match(_findHighlightIndex);

	QModelIndexList selected = selection->selectedIndexes();
	if (selected.size() == 1 && selected.first() == match)
	{
		selection->clearSelection();
	}

	if (selection->currentIndex() == match)
	{
		selection->clearCurrentIndex();
	}
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
	// the highlighted view is being tabbed away from, or its category is closing
	if (event->type() == QEvent::Hide && watched == _findHighlightView)
	{
		clearFindHighlight();
	}

	return QMainWindow::eventFilter(watched, event);
}

void MainWindow::findAgain(bool backwards)
{
	// Nothing to repeat yet, so ask for the text first
	if (_findText.isEmpty())
	{
		menuEditFind();
		return;
	}

	QListView *logs = currentLogView();
	if (!logs)
	{
		QMessageBox::information(this, tr("Find"), tr("There is no log view to search."));
		return;
	}

	if (!findInLogView(logs, backwards))
	{
		QMessageBox::information(this, tr("Find"), tr("\"%1\" not found.").arg(_findText));
	}
}

void MainWindow::menuEditFind()
{
	// Resolved before the dialog is shown, while the log view is still the focused widget, and held
	// through a QPointer because the category could be closed while the dialog is up
	QPointer<QListView> logs = currentLogView();
	if (!logs)
	{
		QMessageBox::information(this, tr("Find"), tr("There is no log view to search."));
		return;
	}

	FindDialog dialog(this, _findText, _findCaseSensitive);
	if (dialog.exec() != QDialog::Accepted) return;

	_findText = dialog.text();
	_findCaseSensitive = dialog.caseSensitive();
	if (_findText.isEmpty()) return;

	if (!findInLogView(logs, false))
	{
		QMessageBox::information(this, tr("Find"), tr("\"%1\" not found.").arg(_findText));
	}
}

void MainWindow::menuEditFindNext()
{
	findAgain(false);
}

void MainWindow::menuEditFindPrevious()
{
	findAgain(true);
}

// Rebuilt from scratch on every change: the word entries sit below the fixed ones, and there are
// never enough of them for rebuilding to be worth avoiding.
void MainWindow::refreshHighlightMenu()
{
	_highlightMenu->clear();

	// The shortcut is set here rather than once at construction because this menu is rebuilt from
	// scratch, so the action carrying it is a new one every time.
	setShortcut(_highlightMenu->addAction("&Add...", this, &MainWindow::menuViewHighlightAdd),
		QKeySequence("Ctrl+K"));
	QAction *clear = _highlightMenu->addAction("&Clear", this, &MainWindow::menuViewHighlightClear);
	clear->setEnabled(!_highlight.isEmpty());

	if (_highlight.isEmpty()) return;

	_highlightMenu->addSeparator();
	for (const QString &word : _highlight.words())
	{
		// & is a mnemonic marker in menu text, so a word containing one has to be doubled up
		QString label(word);
		label.replace("&", "&&");
		_highlightMenu->addAction(label, this, [this, word] {
			_highlight.remove(word);
			refreshHighlightMenu();
			refreshLogViews();
		});
	}
}

// Highlighting is drawn by the delegates, so a word change is a repaint and nothing more. Every
// log view is reachable from _applicationlist, including those in floated windows.
void MainWindow::refreshLogViews()
{
	for (auto &entry : _applicationlist)
	{
		entry.second->updateLogViews();
	}
}

void MainWindow::menuViewHighlightAdd()
{
	bool ok;
	QString word = QInputDialog::getText(this, tr("Add highlight"), tr("Word:"),
		QLineEdit::Normal, QString(), &ok);
	if (!ok) return;

	if (!_highlight.add(word)) return;	// blank, or already listed

	refreshHighlightMenu();
	refreshLogViews();
}

void MainWindow::menuViewHighlightClear()
{
	if (_highlight.isEmpty()) return;

	_highlight.clear();
	refreshHighlightMenu();
	refreshLogViews();
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

void MainWindow::menuViewFont()
{
    bool ok;
    // Seeded from _fontSize rather than qApp->font().pointSize(), which returns -1 when the
    // platform font is specified in pixels rather than points.
    int newSize = QInputDialog::getInt(this, "Change font size", "Font size:",
        _fontSize, MIN_FONT_SIZE, MAX_FONT_SIZE, 2, &ok);
    if (ok)
    {
        applyFontSize(newSize);

        QSettings settings;
        settings.setValue("font_size", newSize);
    }
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

	category->logs->setItemDelegate(new LogDelegate(&_highlight));
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
	category->header = categoryLabel;
	categoryLabel->setAlignment(Qt::AlignCenter);
	categoryLabel->setStyleSheet("QLabel {padding: 4px 0;}");
	QFont font = categoryLabel->font();
	font.setPointSize(_fontSize + HEADER_FONT_OFFSET);
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

void Main_Application::applyFont(const QFont &headerFont)
{
	for (auto &entry : _categorylist)
	{
		auto category = entry.second;
		if (category->header) category->header->setFont(headerFont);
		// LogDelegate::sizeHint derives row height and column widths entirely from the font
		// metrics, and the view caches them: without a relayout the text resizes but the rows
		// keep their old geometry.
		if (category->logs) category->logs->doItemsLayout();
	}
}

void Main_Application::updateLogViews()
{
	for (auto &entry : _categorylist)
	{
		auto category = entry.second;
		// doItemsLayout rather than a viewport repaint: highlighted words are drawn bold, so
		// LogDelegate::sizeHint returns a wider row and the cached geometry has to be redone,
		// otherwise the horizontal scrollbar stops short of the text.
		if (category->logs) category->logs->doItemsLayout();
	}
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
