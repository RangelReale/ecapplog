//
// Data.h
//

# pragma once

#define NOMINMAX

#include <QAbstractListModel>
#include <QDateTime>

#include <memory>
#include <deque>

#define MODELROLE_APP           	Qt::UserRole + 0
#define MODELROLE_TIME          	Qt::UserRole + 1
#define MODELROLE_CATEGORY      	Qt::UserRole + 2
#define MODELROLE_PRIORITY      	Qt::UserRole + 3
#define MODELROLE_MESSAGE       	Qt::UserRole + 4
#define MODELROLE_SOURCE        	Qt::UserRole + 5
#define MODELROLE_ALTAPP        	Qt::UserRole + 10
#define MODELROLE_ALTCATEGORY   	Qt::UserRole + 11
#define MODELROLE_EXTRACATEGORY   	Qt::UserRole + 12

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

class LogModel : public QAbstractListModel
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

	// The longest message this model has held, in characters. LogDelegate picks the column its
	// source field starts at from this, so a category whose lines are all short keeps its source
	// close to the message instead of being spaced out for lines that are in some other tab.
	//
	// Deliberately not lowered when removeLog trims the oldest entries: recomputing it would mean
	// walking the whole list on every trim, and a column that narrowed again as old lines aged out
	// would shift under the reader for no gain. clearLogs is the one thing that resets it.
	int maxMessageLength() const { return _maxMessageLength; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    Qt::DropActions supportedDropActions() const;
private:
    Q_DISABLE_COPY(LogModel)
    QList<std::shared_ptr<LogModelItem> > lst;
    int _maxMessageLength = 0;
};
