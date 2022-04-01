#include "Config.h"
#include "LogModel.h"

#include <QRandomGenerator>

//
// LogModelItem
//
LogModelItem::LogModelItem(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    const QString &message, const QString &source, const QString &altApp, const QString &altCategory, bool isExtraCategory) : 
    _app(appName), _time(time), _category(categoryName), _priority(priority), _message(message), 
    _source(source), _altApp(altApp), _altCategory(altCategory), _isExtraCategory(isExtraCategory)
{
    _prioritycolor = calcPriorityColor();
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

	return QColor(0, 0, 0);
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

//
// LogModel
//
LogModel::LogModel(QObject *parent)
    : QAbstractListModel(parent)
{

}

void LogModel::addLog(const QString &appName, const QDateTime &time, const QString &categoryName, const QString &priority,
    const QString &message, const QString &source, const QString &altApp, const QString &altCategory, bool isExtraCategory)
{
    beginInsertRows(QModelIndex(), 0, 1);
    lst.insert(0, std::make_shared<LogModelItem>(appName, time, categoryName, priority, message, source, 
        altApp, altCategory, isExtraCategory));
    endInsertRows();
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

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= lst.size()) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
		return lst.at(index.row())->getDisplayMessage();
    }

	if (role == MODELROLE_APP) {
        return lst.at(index.row())->getApp();
    } else if (role == MODELROLE_TIME) {
        return lst.at(index.row())->getTime();
    } else if (role == MODELROLE_CATEGORY) {
        return lst.at(index.row())->getCategory();
    } else if (role == MODELROLE_PRIORITY) {
        return lst.at(index.row())->getPriority();
    } else if (role == MODELROLE_MESSAGE) {
        return lst.at(index.row())->getMessage();
    } else if (role == MODELROLE_SOURCE) {
        return lst.at(index.row())->getSource();
    } else if (role == MODELROLE_ALTAPP) {
        return lst.at(index.row())->getAltApp();
    } else if (role == MODELROLE_ALTCATEGORY) {
        return lst.at(index.row())->getAltCategory();
    } else if (role == MODELROLE_EXTRACATEGORY) {
        return lst.at(index.row())->getIsExtraCategory();
    }

	if (role == Qt::ForegroundRole) {
		return lst.at(index.row())->priorityColor();
    }

	// if (role == Qt::BackgroundRole && lst.at(index.row())->getHighlight())
	// 	return QColor((QRandomGenerator::global()->generate() %4)+244, (QRandomGenerator::global()->generate() %4)+244, (QRandomGenerator::global()->generate() %4)+244);

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
