#include "MainWindow.h"
#include "Config.h"
#include "AppIcon.h"
#include "AppInfo.h"
#include "LogDelegate.h"
#include "DetailWindow.h"
#include "FindDialog.h"
#include "Widgets.h"

#include <QApplication>
#include <QScreen>
#include <QMenu>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
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

// Starting widths for a log view whose columns have never been dragged. ALT_COLUMN_WIDTH is the
// floor the two alt fields were drawn at when the row was a single cell; MESSAGE_COLUMN_CHARS, in
// characters of the log font, is the middle of the three widths the message used to step between
// depending on the longest line the tab had held. Both only apply on a first run - after that the
// layout is the one saved under "log_columns".
#define ALT_COLUMN_WIDTH		150
#define MESSAGE_COLUMN_CHARS	80

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

// Where a category tab belongs, lowest first. ALL and ERROR are the two views onto the whole
// application, so they lead, and they are pinned rather than left to arrive in a useful order:
// ALL comes into being only once categories are being grouped, and ERROR with the first error or
// warning, which can be at any point. The remaining all-uppercase categories keep the precedence
// over mixed case ones that they have always had.
int categoryRank(const QString &categoryName)
{
	if (categoryName == Category::CAT_ALL) return 0;
	if (categoryName == Category::CAT_ERROR) return 1;
	return categoryName.isUpper() ? 2 : 3;
}

}


MainWindow *MainWindow::self;

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent), _applicationlist(), _dockCount(0), _fontSize(defaultFontSize()),
	_rootWindow(nullptr), _data(), _server(), _findText(), _findCaseSensitive(false),
	_lastLogView(), _findHighlightView(), _findHighlightIndex(), _findHighlightShown(false),
	_logColumnState(), _logColumnSaveTimer(nullptr), _highlight(), _highlightMenu(nullptr)
{
	MainWindow::self = this;
	QSettings settings;

	// A fixed default rather than a fraction of the screen, and deliberately wide: a log row is a
	// timestamp, a priority tag, the message and then the source, so width is what stops the
	// horizontal scrollbar being needed from the very first line (the starting column widths are in
	// applyLogColumns). Nothing else here decides the startup size - an explicit geometry bypasses
	// sizeHint entirely, and the docking system reports a flat 60x40 minimum for its dock widgets
	// whatever they contain.
	//
	// boundedTo keeps the window on a panel smaller than the one this was chosen on, and
	// availableGeometry excludes the menu bar and the Dock, which is why centring here needs none of
	// the title bar fudge the old top-left placement did.
	const QRect available = screen()->availableGeometry();
	resize(QSize(1280, 800).boundedTo(available.size()));
	move(available.center() - rect().center());

	refreshWindowTitle();

	// settings
	_data.setGroupCategories(settings.value("group_categories", false).toBool());
	// clamped to the range menuViewFont offers, so a hand-edited value cannot produce an
	// unusable window
	_fontSize = qBound(MIN_FONT_SIZE, settings.value("font_size", defaultFontSize()).toInt(),
		MAX_FONT_SIZE);
	// Not validated here the way the two above are: QHeaderView::restoreState checks the blob it is
	// given against the column count and refuses anything it does not recognise, and applyLogColumns
	// falls back to the measured defaults when it does.
	_logColumnState = settings.value("log_columns").toByteArray();

	_logColumnSaveTimer = new QTimer(this);
	_logColumnSaveTimer->setSingleShot(true);
	_logColumnSaveTimer->setInterval(500);
	connect(_logColumnSaveTimer, &QTimer::timeout, this, [this] {
		QSettings s;
		s.setValue("log_columns", _logColumnState);
	});

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
	// itself never clobbers this: a QLineEdit is not a QTreeView. Neither is a QHeaderView, and it
	// sets Qt::NoFocus on itself anyway, so dragging a column does not disturb this either.
	connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
		if (QTreeView *view = qobject_cast<QTreeView*>(now)) _lastLogView = view;
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

	// menu: HELP
	// AboutRole is what macOS acts on: Qt lifts the item out of whichever menu it was put in and
	// into the application menu, where a Mac expects to find it. Set explicitly rather than left to
	// the text heuristic, and set before the action reaches a menu, since the platform menu item is
	// built at the moment it is added and does not pick the role up afterwards.
	QAction *aboutAction = new QAction("&About ECAppLog", this);
	aboutAction->setMenuRole(QAction::AboutRole);
	connect(aboutAction, &QAction::triggered, this, &MainWindow::menuHelpAbout);
#ifdef Q_OS_DARWIN
	// Parented to Edit rather than to a Help menu of its own, which is not the arbitrary choice it
	// looks like: Qt discards a menu once every one of its items has been merged away, and it does
	// so *before* the merge, so an About item alone in a Help menu is dropped along with the menu
	// and never reaches the application menu at all. Any menu with other items in it works, and the
	// role - not the parent - is what decides where the item ends up, so Edit keeps its own
	// contents and macOS still gets ECAppLog -> About ECAppLog.
	editMenu->addAction(aboutAction);
#else
	QMenu *helpMenu = new QMenu("&Help", this);
	helpMenu->addAction(aboutAction);

	menuBar()->addMenu(helpMenu);
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
	// Worked out before _fontSize is overwritten. Column widths are pixels sized for the old text,
	// so they are scaled rather than left alone or recomputed: the proportions are the ones the
	// user dragged, and leaving them would mean re-dragging six columns in every open tab after
	// one font change.
	const double columnScale = (_fontSize > 0 && pointSize > 0)
		? static_cast<double>(pointSize) / static_cast<double>(_fontSize) : 1.0;

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
		entry.second->applyFont(headerFont, columnScale);
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

void MainWindow::logListDetail(QTreeView *logs)
{
	QStringList slist;
	QStringList rslist;

	// Oldest first: LogModel inserts the newest entry at row 0, so the selection has to be
	// walked backwards to read in the order the entries happened.
	//
	// selectedRows, not selectedIndexes: the model has a cell per column now, and
	// selectedIndexes would hand back every one of them - the same entry repeated once per
	// column, in the clipboard and in the detail window alike.
	QListIterator<QModelIndex> iter(logs->selectionModel()->selectedRows());
	iter.toBack();
	while (iter.hasPrevious()) {
		auto index(iter.previous());
		slist.append(index.data(MODELROLE_ROWTEXT).toString());

		// Raw, and only when there is one - DetailWindow decides how to format each of these
		// for its SOURCE tab, and empty ones would just become blank gaps in it.
		const QString source(index.data(MODELROLE_SOURCE).toString());
		if (!source.isEmpty())
			rslist.append(source);
	}

	DetailWindow *dwin = new DetailWindow(nullptr, slist.join("\n"), rslist);
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
QTreeView *MainWindow::currentLogView() const
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

	return categoryParent->findChild<QTreeView*>();
}

// Searches the message text of every row exactly once, starting after (or before, when going
// backwards) the current one and wrapping around, so a failure means there really is no match.
bool MainWindow::findInLogView(QTreeView *logs, bool backwards)
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
		// The source is searched too, now that the row shows it. It is searched in full, so a match
		// can sit past the point the row elides it at - the entry is selected with nothing visibly
		// matching in it, and the detail window's SOURCE tab is where to read the rest.
		if (index.data(MODELROLE_MESSAGE).toString().contains(_findText, caseSensitivity)
			|| index.data(MODELROLE_SOURCE).toString().contains(_findText, caseSensitivity))
		{
			clearFindHighlight();
			// the focus keeps the highlight from being taken back straight away, and lets the
			// repeat shortcuts and the arrow keys carry on from the match
			logs->setFocus();
			// Deliberately not QAbstractItemView::setCurrentIndex, which asks selectionCommand()
			// what to do and that reads the modifiers being held right now: the Command in Cmd+G
			// is Qt::ControlModifier, so it would answer Toggle and pile every match onto the
			// selection instead of replacing it (Shift+Cmd+G would extend a range).
			//
			// Rows, spelled out, is the price of going round the view: selectionCommand() is also
			// where a view adds its own selectionBehaviour, so without this the match selects the
			// one cell the index names instead of the whole entry - and selectedRows(), which is
			// what clearFindHighlight and the clipboard read, only counts a fully selected row.
			logs->selectionModel()->setCurrentIndex(index,
				QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
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

	QTreeView *view = _findHighlightView;
	if (!view) return;
	view->removeEventFilter(this);

	QItemSelectionModel *selection = view->selectionModel();
	if (!selection || !_findHighlightIndex.isValid()) return;

	QModelIndex match(_findHighlightIndex);

	// selectedRows so that one selected entry counts as one, however many columns it spans
	QModelIndexList selected = selection->selectedRows();
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

	QTreeView *logs = currentLogView();
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
	QPointer<QTreeView> logs = currentLogView();
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

void MainWindow::menuHelpAbout()
{
	// The Qt version is two facts, not one: QT_VERSION_STR is what the binary was compiled
	// against and qVersion() is what it actually loaded. They diverge whenever a deployed build
	// picks up a different Qt than it was built with, which is worth being able to read off a bug
	// report - so the running version is only spelled out when it disagrees.
	QString qtVersion(QT_VERSION_STR);
	if (qtVersion != QLatin1String(qVersion()))
		qtVersion = tr("%1 (running %2)").arg(qtVersion, QString::fromLatin1(qVersion()));

	// Rich text so the address and the URL are clickable. The two anchors are concatenated rather
	// than built with arg(): each of them names its target twice, once in the href and once as the
	// visible text, and a repeated place marker is a needless thing to depend on.
	const QString email(ECAPPLOG_AUTHOR_EMAIL);
	const QString url(ECAPPLOG_URL);
	const QString emailLink = "<a href=\"mailto:" + email + "\">" + email + "</a>";
	const QString urlLink = "<a href=\"" + url + "\">" + url + "</a>";

	QString details;
	details += QString("<p>%1</p>")
		.arg(tr("A networked logging GUI, for programmers debugging code on a local computer."));
	details += QString("<p>%1<br>%2</p>")
		.arg(tr("Author: %1").arg(ECAPPLOG_AUTHOR_NAME), emailLink);
	details += "<p>" + urlLink + "</p>";
	details += QString("<p>%1</p>").arg(tr("Built with Qt %1").arg(qtVersion));

	// Assembled by hand rather than handed to QMessageBox, which cannot be made to do this one.
	// Its informative label is where the two anchors would live, and it is constructed without a
	// parent and only adopted when the box is laid out - so it cannot be reached through
	// findChildren() beforehand, and whatever link handling it is given at creation is what it
	// keeps. QMessageBox::setTextInteractionFlags does not reach it either. A QDialog of our own
	// is both shorter to reason about and the pattern FindDialog already uses.
	QDialog about(this);
	about.setWindowTitle(tr("About %1").arg(QApplication::applicationName()));

	// The application artwork rather than a generic glyph. appIcon() is multi-resolution, so
	// pixmap() picks the right master for the size asked for.
	QLabel *iconLabel = new QLabel;
	iconLabel->setPixmap(appIcon().pixmap(64, 64));

	QLabel *titleLabel = new QLabel(QString("%1 %2")
		.arg(QApplication::applicationName(), QString(ECAPPLOG_VERSION)));
	QFont titleFont = titleLabel->font();
	titleFont.setBold(true);
	titleLabel->setFont(titleFont);

	QLabel *detailsLabel = new QLabel(details);
	detailsLabel->setTextFormat(Qt::RichText);
	detailsLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
	// Without this a click on an anchor only emits linkActivated, which nothing is connected to.
	detailsLabel->setOpenExternalLinks(true);
	detailsLabel->setWordWrap(true);
	// Wide enough that the repository URL stays on one line; the description wraps to suit.
	detailsLabel->setMinimumWidth(340);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
	connect(buttons, &QDialogButtonBox::accepted, &about, &QDialog::accept);

	// The icon column carries a stretch so the artwork stays beside the first line rather than
	// centring itself against the whole text block.
	QVBoxLayout *iconColumn = new QVBoxLayout;
	iconColumn->addWidget(iconLabel);
	iconColumn->addStretch();

	QVBoxLayout *textColumn = new QVBoxLayout;
	textColumn->addWidget(titleLabel);
	textColumn->addWidget(detailsLabel);
	textColumn->addStretch();

	QHBoxLayout *body = new QHBoxLayout;
	body->addLayout(iconColumn);
	body->addSpacing(12);
	body->addLayout(textColumn);

	QVBoxLayout *layout = new QVBoxLayout(&about);
	layout->addLayout(body);
	layout->addWidget(buttons);

	about.exec();
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
	QTreeView *list = qobject_cast<QTreeView*>(sender());	

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
			QListIterator<QModelIndex> iter(list->selectionModel()->selectedRows());
			iter.toBack();
			while (iter.hasPrevious()) {
				slist.append(iter.previous().data(MODELROLE_ROWTEXT).toString());
			}
			QGuiApplication::clipboard()->setText(slist.join("\n"));
		} 
		else if (selectedItem == acopyrs) 
		{
			QStringList slist;
			QListIterator<QModelIndex> iter(list->selectionModel()->selectedRows());
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
	QTreeView *list = qobject_cast<QTreeView*>(sender());
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
	TabWidget *categories = new TabWidget;
	// The category strip is the one that fills up, so it gets the tab counter.
	categories->enableTabList();

	auto app = std::make_shared<Main_Application>(appName, categories);
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

// The shape the log list has always had: one line per entry, whole rows selected at once, and no
// tree furniture. What a QTreeView adds over the QListView this replaced is the header - the
// columns used to be pixel offsets LogDelegate worked out for itself, and the widest message a tab
// had ever held decided where the source started for every row in it.
void MainWindow::configureLogView(QTreeView *logs)
{
	logs->setRootIsDecorated(false);
	logs->setItemsExpandable(false);
	logs->setIndentation(0);
	logs->setUniformRowHeights(true);	// every row is one line of the log font
	logs->setAllColumnsShowFocus(true);
	logs->setWordWrap(false);
	logs->setTextElideMode(Qt::ElideRight);
	logs->setContextMenuPolicy(Qt::CustomContextMenu);

	// Rows, not cells: the context menu, the clipboard, the detail window and Find all act on whole
	// entries, and selectedRows only means anything under this behaviour.
	logs->setSelectionBehavior(QAbstractItemView::SelectRows);
	logs->setSelectionMode(QAbstractItemView::ExtendedSelection);

	// The model is a stream in arrival order and has no other ordering to offer, so a heading must
	// not look like something that could be sorted by.
	logs->setSortingEnabled(false);
	logs->header()->setSortIndicatorShown(false);
	logs->header()->setSectionsClickable(false);

	logs->header()->setSectionsMovable(true);
	logs->header()->setSectionResizeMode(QHeaderView::Interactive);
	// Deliberately not stretched. The source is the field most likely to want more room than the
	// window has, and stretching the last section both suppresses the horizontal scrollbar and
	// makes the width saved for that column mean nothing.
	logs->header()->setStretchLastSection(false);
}

// Gives a new view the shared column layout. Called after setModel: a header has no sections to
// size until it knows how many columns there are, and restoreState against the wrong count is
// dropped without a word.
void MainWindow::applyLogColumns(QTreeView *logs)
{
	QHeaderView *header = logs->header();

	if (!_logColumnState.isEmpty() && header->restoreState(_logColumnState)) return;

	// First run. The two fixed fields are measured from the widest text they can hold rather than
	// from a run of X the way LogDelegate used to measure them: the old row drew a field that
	// simply overran into the next one, where a column elides instead, and a timestamp cut short
	// to "2026-08-20 09:02:20.4..." is the one thing this whole change exists to stop happening.
	// Digits are tabular in the fonts these platforms default to, so the zeroes measure true.
	//
	// The two text fields get MESSAGE_COLUMN_CHARS, the middle of the three widths the message
	// column used to step between depending on the longest line its tab had held.
	const QFontMetrics metrics(logs->font());
	const int padding = metrics.horizontalAdvance("XX");	// what a cell leaves at either edge
	header->resizeSection(LOGCOL_TIME,
		metrics.horizontalAdvance("0000-00-00 00:00:00.000") + padding);
	header->resizeSection(LOGCOL_PRIORITY,
		metrics.horizontalAdvance(QString("[%1]").arg(Priority::PRIO_INFORMATION)) + padding);
	header->resizeSection(LOGCOL_APP, ALT_COLUMN_WIDTH);
	header->resizeSection(LOGCOL_CATEGORY, ALT_COLUMN_WIDTH);
	header->resizeSection(LOGCOL_MESSAGE, metrics.averageCharWidth() * MESSAGE_COLUMN_CHARS);
	header->resizeSection(LOGCOL_SOURCE, metrics.averageCharWidth() * MESSAGE_COLUMN_CHARS);
}

void MainWindow::saveLogColumns(QHeaderView *header)
{
	_logColumnState = header->saveState();

	// Debounced. sectionResized fires once per pixel of a drag, and serialising the whole header
	// into QSettings at that rate is work nobody asked for; the in-memory copy above is what a new
	// view reads, so nothing is waiting on the write.
	_logColumnSaveTimer->start();
}

// The two alt columns stay hidden until the view has an entry that carries them, so an ordinary
// category tab - which never has either - looks the way it did before they were columns.
//
// Applied after applyLogColumns, and again whenever the model says a field has appeared. That
// order matters: saveState records which sections were hidden, so a layout captured from a tab
// without these fields would otherwise keep them hidden in a tab that has them.
void MainWindow::refreshAltColumns(QTreeView *logs)
{
	const LogModel *model = qobject_cast<const LogModel*>(logs->model());
	if (!model) return;

	QHeaderView *header = logs->header();
	const bool show[] = { model->hasAltApp(), model->hasAltCategory() };
	const int columns[] = { LOGCOL_APP, LOGCOL_CATEGORY };

	for (int i = 0; i < 2; ++i)
	{
		header->setSectionHidden(columns[i], !show[i]);
		// A section un-hidden from a saved state gets back the width it was hidden at, which is
		// zero if it was never given one - an invisible column the user has to find the edge of.
		if (show[i] && header->sectionSize(columns[i]) < 1)
			header->resizeSection(columns[i], ALT_COLUMN_WIDTH);
	}
}

void MainWindow::onNewCategory(const QString &appName, const QString &categoryName, QAbstractItemModel *model)
{
	auto findapp = _applicationlist.find(appName);
	if (findapp == _applicationlist.end()) return;
	auto app = findapp->second;

	auto category = std::make_shared<Main_Category>(categoryName, app->categories, new QTreeView, new QLabel("-----"));
	app->addCategory(category);

	category->logs->setProperty(PROPERTY_APPNAME, appName);	
	category->logs->setProperty(PROPERTY_CATEGORYNAME, categoryName);	

	configureLogView(category->logs);
	category->logs->setItemDelegate(new LogDelegate(&_highlight));

	category->logs->setModel(model);

	// After setModel: a header has no sections to size or hide until it knows how many columns
	// there are, and restoreState against the wrong count is silently dropped.
	applyLogColumns(category->logs);
	refreshAltColumns(category->logs);
	if (LogModel *logModel = qobject_cast<LogModel*>(model))
	{
		QTreeView *logs = category->logs;
		connect(logModel, &LogModel::altColumnsChanged, logs, [this, logs] { refreshAltColumns(logs); });
	}

	// Saved from the header rather than from the view, so a section dragged in any tab becomes the
	// layout every tab opened afterwards starts with.
	connect(category->logs->header(), &QHeaderView::sectionResized, this,
		[this, logs = category->logs](int, int, int) { saveLogColumns(logs->header()); });
	connect(category->logs->header(), &QHeaderView::sectionMoved, this,
		[this, logs = category->logs](int, int, int) { saveLogColumns(logs->header()); });

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

	// Inserted ahead of the first tab that ranks after this one, so the category joins its own group
	// without displacing what is already there. Reading the rank back from the tab text keeps this
	// working after the tabs have been reordered by hand.
	const int rank = categoryRank(categoryName);
	int idx = app->categories->count();
	for (int i = 0; i < idx; ++i)
	{
		if (categoryRank(app->categories->tabText(i)) > rank)
		{
			idx = i;
			break;
		}
	}
	idx = app->categories->insertTab(idx, categoryParent, categoryName);
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

void Main_Application::applyFont(const QFont &headerFont, double columnScale)
{
	for (auto &entry : _categorylist)
	{
		auto category = entry.second;
		if (category->header) category->header->setFont(headerFont);
		if (!category->logs) continue;

		if (!qFuzzyCompare(columnScale, 1.0))
		{
			QHeaderView *header = category->logs->header();
			for (int col = 0; col < header->count(); ++col)
			{
				if (header->isSectionHidden(col)) continue;
				header->resizeSection(col, qRound(header->sectionSize(col) * columnScale));
			}
		}

		// LogDelegate::sizeHint derives the row height from the font metrics, and the view caches
		// it: without a relayout the text resizes but the rows keep their old geometry.
		category->logs->doItemsLayout();
	}
}

void Main_Application::updateLogViews()
{
	for (auto &entry : _categorylist)
	{
		// A repaint is enough: highlighted words are drawn bold, which used to make
		// LogDelegate::sizeHint return a wider row that the cached geometry had to be redone for.
		// Column widths belong to the header now, so nothing about the layout has changed.
		if (entry.second->logs) entry.second->logs->viewport()->update();
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
