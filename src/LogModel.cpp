#include "Config.h"
#include "LogModel.h"

#include <QFont>
#include <QRandomGenerator>

//
// LogModelItem
//
LogModelItem::LogModelItem(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    const QString &message, const QString &source, const QString &altApp, const QString &altCategory, bool isExtraCategory,
    const LogOptions& logOptions) :
    _app(appName), _time(time), _category(categoryName), _priority(priority), _message(message),
    _source(source), _altApp(altApp), _altCategory(altCategory), _isExtraCategory(isExtraCategory), _bgColor()
{
    _prioritycolor = calcPriorityColor();

    if (!logOptions.color.isEmpty())
    {
        QColor c(logOptions.color);
        if (c.isValid())
        {
            _prioritycolor = c;
        }
    }
    if (!logOptions.bgColor.isEmpty())
    {
        QColor c(logOptions.bgColor);
        if (c.isValid())
        {
            _bgColor = c;
        }
    }
}

QColor LogModelItem::calcPriorityColor() const
{
	if (_priority == Priority::PRIO_TRACE || _priority == Priority::PRIO_DEBUG)
		return QColor((QRandomGenerator::global()->generate() % 55) + 160, (QRandomGenerator::global()->generate() % 55) + 160, 0);

	if (_priority == Priority::PRIO_INFORMATION || _priority == Priority::PRIO_NOTICE)
		return QColor(0, (QRandomGenerator::global()->generate() % 55) + 200, 0);

	if (_priority == Priority::PRIO_WARNING)
		return QColor(0, 0, (QRandomGenerator::global()->generate() % 55) + 200);

	if (_priority == Priority::PRIO_FATAL || _priority == Priority::PRIO_CRITICAL || _priority == Priority::PRIO_ERROR)
		return QColor((QRandomGenerator::global()->generate() % 55) + 200, 0, 0);

	// Invalid on purpose: an unknown priority has no colour of its own, and hardcoding black made
	// those lines unreadable under a dark theme. data() omits ForegroundRole when this is invalid,
	// which leaves the view to use its own palette.
	return QColor();
}

QString LogModelItem::getDisplayMessage() const
{
	return QString("%1 [%2]%3%4 %5").
		arg(_time.toLocalTime().toString("yyyy-MM-dd hh:mm:ss.zzz")).
		arg(_priority).
        arg("").
		//arg(logSourceTab ? QString(" [[%1]]").arg(f_source) : "").
		arg(!_altCategory.isEmpty() ? QString(" {%1}").arg(_altCategory) : "").
		arg(_message);
}

// The braces around the two alt fields are kept from when the whole row was drawn as one cell.
// They still earn their place: those fields say where an entry was redirected from rather than
// anything the sending program wrote, and a bare name under a heading reads as the program's own.
QString LogModelItem::getColumnText(int column) const
{
	switch (column)
	{
	case LOGCOL_TIME:
		return _time.toLocalTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
	case LOGCOL_PRIORITY:
		return QString("[%1]").arg(_priority);
	case LOGCOL_APP:
		return _altApp.isEmpty() ? QString() : QString("{%1}").arg(_altApp);
	case LOGCOL_CATEGORY:
		return _altCategory.isEmpty() ? QString() : QString("{%1}").arg(_altCategory);
	case LOGCOL_MESSAGE:
		return _message;
	case LOGCOL_SOURCE:
		return _source;
	}

	return QString();
}

//
// LogModel
//
LogModel::LogModel(QObject *parent)
    : QAbstractTableModel(parent)
{

}

bool LogModel::noteAltColumns(const std::shared_ptr<LogModelItem> &item)
{
    bool changed = false;

    if (!_hasAltApp && !item->getAltApp().isEmpty())
    {
        _hasAltApp = true;
        changed = true;
    }
    if (!_hasAltCategory && !item->getAltCategory().isEmpty())
    {
        _hasAltCategory = true;
        changed = true;
    }

    return changed;
}

void LogModel::addLog(std::shared_ptr<LogModelItem> item)
{
    beginInsertRows(QModelIndex(), 0, 0);
    lst.insert(0, item);
    const bool altChanged = noteAltColumns(item);
    endInsertRows();

    // Emitted after endInsertRows, never during: what listens to this un-hides a header section,
    // and reshaping a view while its model is still mid-insertion is not something
    // QAbstractItemModel promises to survive.
    if (altChanged) emit altColumnsChanged();
}

void LogModel::clearLogs()
{
    const bool altChanged = _hasAltApp || _hasAltCategory;

    beginResetModel();
    lst.clear();
    _hasAltApp = false;
    _hasAltCategory = false;
    endResetModel();

    if (altChanged) emit altColumnsChanged();
}

void LogModel::addLog(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    const QString &message, const QString &source, const QString &altApp, const QString &altCategory, bool isExtraCategory)
{
    addLog(std::make_shared<LogModelItem>(appName, time, categoryName, priority, message, source,
        altApp, altCategory, isExtraCategory));
}

void LogModel::addLogs(const std::deque<std::shared_ptr<LogModelItem>>& item_list)
{
    if (item_list.empty()) return;

    bool altChanged = false;

    beginInsertRows(QModelIndex(), 0, static_cast<int>(item_list.size() - 1));
    for (auto item : item_list)
    {
        lst.insert(0, item);
        if (noteAltColumns(item)) altChanged = true;
    }
    endInsertRows();

    if (altChanged) emit altColumnsChanged();
}

void LogModel::removeLog(int amount)
{
    if (amount > lst.count()) amount = lst.count();

    beginRemoveRows(QModelIndex(), lst.count()-amount, lst.count()-1);

    for (int r = 0; r < amount; ++r)
        lst.removeLast();

    endRemoveRows();
}

int LogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return lst.count();
}

int LogModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return LOGCOL_COUNT;
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= lst.size()) {
        return QVariant();
    }

    const std::shared_ptr<LogModelItem> &item = lst.at(index.row());

    // The one column-aware role. Everything below describes the entry rather than the cell, so a
    // row can still be identified by its column 0 index alone.
    if (role == Qt::DisplayRole) {
        return item->getColumnText(index.column());
    }

    if (role == Qt::TextAlignmentRole) {
        // The message and the source are the two that can outgrow their column, so they start at
        // its left edge and elide to the right. The other four are short and fixed-width, and a
        // timestamp or a priority reads better centred under its heading.
        switch (index.column())
        {
        case LOGCOL_MESSAGE:
        case LOGCOL_SOURCE:
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        default:
            return static_cast<int>(Qt::AlignCenter);
        }
    }

    if (role == Qt::FontRole) {
        // Italic for the two redirection fields, as they were drawn when the row was a single
        // cell. Only the style is set, so QItemDelegate::setOptions resolves family and size from
        // the font of the view and View -> Font size keeps working.
        if (index.column() == LOGCOL_APP || index.column() == LOGCOL_CATEGORY)
        {
            QFont font;
            font.setItalic(true);
            return font;
        }

        return QVariant();
    }

	if (role == MODELROLE_APP) {
        return item->getApp();
    } else if (role == MODELROLE_TIME) {
        return item->getTime();
    } else if (role == MODELROLE_CATEGORY) {
        return item->getCategory();
    } else if (role == MODELROLE_PRIORITY) {
        return item->getPriority();
    } else if (role == MODELROLE_MESSAGE) {
        return item->getMessage();
    } else if (role == MODELROLE_SOURCE) {
        return item->getSource();
    } else if (role == MODELROLE_ROWTEXT) {
        // The whole entry as one line, for the clipboard and the detail window. Qt::DisplayRole
        // was this before the columns were split out of it.
        return item->getDisplayMessage();
    } else if (role == MODELROLE_ALTAPP) {
        return item->getAltApp();
    } else if (role == MODELROLE_ALTCATEGORY) {
        return item->getAltCategory();
    } else if (role == MODELROLE_EXTRACATEGORY) {
        return item->getIsExtraCategory();
    }

	if (role == Qt::ForegroundRole) {
		if (item->priorityColor().isValid()) return item->priorityColor();
    }
    else if (role == Qt::BackgroundRole) {
        if (item->bgColor().isValid()) return item->bgColor();
    }

    return QVariant();
}

QVariant LogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section)
    {
    case LOGCOL_TIME:		return tr("Time");
    case LOGCOL_PRIORITY:	return tr("Priority");
    case LOGCOL_APP:		return tr("Application");
    case LOGCOL_CATEGORY:	return tr("Category");
    case LOGCOL_MESSAGE:	return tr("Message");
    case LOGCOL_SOURCE:		return tr("Source");
    }

    return QVariant();
}

Qt::ItemFlags LogModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled;

    return QAbstractItemModel::flags(index) | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

Qt::DropActions LogModel::supportedDropActions() const
{
    return QAbstractItemModel::supportedDropActions() | Qt::MoveAction;
}
