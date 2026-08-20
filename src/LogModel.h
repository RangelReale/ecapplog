//
// Data.h
//

# pragma once

#define NOMINMAX

#include <QAbstractTableModel>
#include <QDateTime>

#include <memory>
#include <deque>

#define MODELROLE_APP           	Qt::UserRole + 0
#define MODELROLE_TIME          	Qt::UserRole + 1
#define MODELROLE_CATEGORY      	Qt::UserRole + 2
#define MODELROLE_PRIORITY      	Qt::UserRole + 3
#define MODELROLE_MESSAGE       	Qt::UserRole + 4
#define MODELROLE_SOURCE        	Qt::UserRole + 5
#define MODELROLE_ROWTEXT       	Qt::UserRole + 6
#define MODELROLE_ALTAPP        	Qt::UserRole + 10
#define MODELROLE_ALTCATEGORY   	Qt::UserRole + 11
#define MODELROLE_EXTRACATEGORY   	Qt::UserRole + 12

// The columns of a log view, in the order the model creates them. The header is movable, so the
// visual order is the user's and nothing may assume it matches this.
//
// Every MODELROLE_ above answers the same for any column of a row - they describe the entry, not
// the cell - so index(row, 0) still identifies an entry on its own. Find, "Copy source" and the
// detail window all depend on that, and it is why only Qt::DisplayRole is column-aware.
enum LogColumn
{
	LOGCOL_TIME = 0,
	LOGCOL_PRIORITY,
	LOGCOL_APP,
	LOGCOL_CATEGORY,
	LOGCOL_MESSAGE,
	LOGCOL_SOURCE,
	LOGCOL_COUNT
};

class LogOptions
{
public:
	LogOptions() : color(), bgColor() {}
	LogOptions(const QString &color, const QString &bgColor = "") : color(color), bgColor(bgColor) {}

	QString color;
	QString bgColor;
};

class LogModelItem
{
public:
	LogModelItem(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    	const QString &message, const QString &source, const QString &altApp = QString(), 
		const QString &altCategory = QString(), bool isExtraCategory = false, 
		const LogOptions& logOptions = LogOptions());

	const QString &getApp() const { return _app; }
	const QDateTime &getTime() const { return _time; }
	const QString &getCategory() const { return _category; }
	const QString &getPriority() const { return _priority; }
	const QString &getMessage() const { return _message; }
	const QString &getSource() const { return _source; }
	const QString &getAltApp() const { return _altApp; }
	const QString &getAltCategory() const { return _altCategory; }
	bool getIsExtraCategory() const { return _isExtraCategory; }

	QString getDisplayMessage() const;

	// What one column of this entry reads as. The formatting lives here rather than in
	// LogDelegate so that the header, a tooltip and anything else that asks the model for text
	// gets the same string the row is drawn from.
	QString getColumnText(int column) const;

	QColor priorityColor() const { return _prioritycolor; }
	QColor bgColor() const { return _bgColor; }
private:
	QColor calcPriorityColor() const;

	QString _app;
	QDateTime _time;
	QString _category;
	QString _priority;
	QString _message;
	QString _source;
	QString _altApp;
	QString _altCategory;
	bool _isExtraCategory;

	QColor _prioritycolor, _bgColor;	
};

class LogModel : public QAbstractTableModel
{
public:
    Q_OBJECT
public:
    explicit LogModel(QObject *parent = 0);

	void addLog(std::shared_ptr<LogModelItem> item);
	void addLog(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    	const QString &message, const QString &source, const QString &altApp, const QString &altCategory, bool isExtraCategory);
	void removeLog(int amount);
	void addLogs(const std::deque<std::shared_ptr<LogModelItem>> &item_list);
	void clearLogs();

	// Whether any entry this model has held carried an alt app or an alt category. The view keeps
	// those two columns hidden until one does, so an ordinary category tab - which never has
	// either - looks the way it did before the columns existed.
	//
	// Deliberately not lowered when removeLog trims the oldest entries, for the same reason the
	// column widths are the user's: a column that vanished again as old lines aged out would take
	// its width with it and reflow the whole view under the reader. clearLogs resets them.
	bool hasAltApp() const { return _hasAltApp; }
	bool hasAltCategory() const { return _hasAltCategory; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    Qt::DropActions supportedDropActions() const;
Q_SIGNALS:
	// One of the two flags above has just turned true. A signal rather than leaving the view to
	// re-test on rowsInserted, which would repeat the check on every batch for the whole life of
	// a tab that never carries these fields - which is most of them.
	void altColumnsChanged();
private:
    Q_DISABLE_COPY(LogModel)
	// Records an entry's alt fields. Returns whether either flag was newly turned on, so the
	// caller can emit altColumnsChanged once the insertion it is part of has finished.
	bool noteAltColumns(const std::shared_ptr<LogModelItem> &item);

    QList<std::shared_ptr<LogModelItem> > lst;
    bool _hasAltApp = false;
    bool _hasAltCategory = false;
};
