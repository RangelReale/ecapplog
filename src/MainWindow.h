//
// MainWindow.h
//

#pragma once

#define NOMINMAX

#include "Server.h"
#include "Data.h"
#include "Highlight.h"

#include "DockManager.h"

#include <QTreeView>
#include <QHeaderView>
#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QPointer>
#include <QPersistentModelIndex>

#include <string>
#include <map>

class QTimer;

class Main_Category : public QObject
{
	Q_OBJECT
public:
	Main_Category(const QString &name, QTabWidget *apptabs, QTreeView *logs, QLabel *logsamount) :
		name(name), apptabs(apptabs), logs(logs), logsamount(logsamount), header(nullptr) {}

	QString name;
	QTabWidget *apptabs;
	QTreeView *logs;
	QLabel *logsamount;
	QLabel *header;		// retained so the font size can be kept in step with the log text
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
	void applyFont(const QFont &headerFont, double columnScale);
	void updateLogViews();
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
	void onJsonReceived(const QString &appName, quint8 cmd, const QJsonObject &jsonData);
	void onJsonError(const QString& appName, const QJsonParseError &error);
	void onError(const QString& appName, const QString &error);

    void onNewApplication(const QString &appName);
    void onDelApplication(const QString &appName);
    void onNewCategory(const QString &appName, const QString &categoryName, QAbstractItemModel *model);
    void onDelCategory(const QString &appName, const QString &categoryName);
	void onLogAmount(const QString &appName, const QString &categoryName, int amount);
	void onLogItemsPerSecond(const QString& appName, const QString& categoryName, double itemsPerSecond);
	void onNewFilter(const QString &filterName);
    void onFilterChanged(const QString &filterName);

	QTabWidget *createWindow();
	void logListDetail(QTreeView *logs);

	void menuEditClear();
	void menuEditPause();
	void menuEditFind();
	void menuEditFindNext();
	void menuEditFindPrevious();
    void menuViewFont();
	void menuViewGroupCategories();
	void menuViewNewWindow();
	void menuViewHighlightAdd();
	void menuViewHighlightClear();
	void menuFilterNew();
	void menuFilterClear();
	void menuFilterGroupBy();
	void menuHelpAbout();

#ifdef ECAPPLOG_DEBUG_MENUS
	void menuDebugPublishLogs();
#endif

	void refreshWindowTitle();
	void applyFontSize(int pointSize);

	void applicationTabClose(int index);
	void applicationTabBarContextMenu(const QPoint &point);

	void categoryTabClose(int index);
	void categoryTabBarContextMenu(const QPoint &point);

	void logListContextMenu(const QPoint &point);
	void logListDoubleClicked(const QModelIndex&);
protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
private:
	void onCmdLog(const QString& appName, const QJsonObject &jsonData);

	QTreeView *currentLogView() const;
	bool findInLogView(QTreeView *logs, bool backwards);
	void findAgain(bool backwards);
	void clearFindHighlight();

	// Puts a newly created log view into the flat, one-line-per-row shape the log list has always
	// had, and gives it the header the columns are resized from.
	void configureLogView(QTreeView *logs);
	// Column widths and order. Every view shares one layout, so a category opened later joins the
	// tabs already on screen instead of starting over.
	void applyLogColumns(QTreeView *logs);
	void saveLogColumns(QHeaderView *header);
	// Hides the two alt columns until the view has an entry that carries them.
	void refreshAltColumns(QTreeView *logs);

	void refreshHighlightMenu();
	void refreshLogViews();

	Server _server;
	Data _data;
	int _dockCount;
	int _fontSize;

	// Find state, kept for the session only (unlike font size, it is not worth persisting)
	QString _findText;
	bool _findCaseSensitive;
	// QPointer: log views are destroyed whenever their category is closed or cleared
	QPointer<QTreeView> _lastLogView;
	// The row the last search landed on. Kept even after its highlight is taken back, so a repeated
	// search carries on from there instead of starting over.
	QPointer<QTreeView> _findHighlightView;
	QPersistentModelIndex _findHighlightIndex;
	bool _findHighlightShown;

	// The shared log column layout, as QHeaderView::saveState writes it, persisted under
	// "log_columns". Held here as well as in QSettings because it is read once per new view and
	// written on every pixel of a drag.
	QByteArray _logColumnState;
	QTimer *_logColumnSaveTimer;

	// Words marked in every log list. Handed to each LogDelegate by pointer, so it must outlive
	// the views, and it does: MainWindow owns them all.
	Highlight _highlight;

	typedef std::map<QString, std::shared_ptr<Main_Application> > applicationlist_t;
	applicationlist_t _applicationlist;

	static MainWindow *self;
	ads::CDockManager* _dockManager;
	ads::CDockWidget* _rootWindow;
	QMenu *_viewMenu;
	QMenu *_filterMenu;
	QMenu *_highlightMenu;	// rebuilt whenever the word list changes
};
