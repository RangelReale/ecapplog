#include "Config.h"
#include "LogModel.h"

#include <QRandomGenerator>

//
// LogModelItem
//
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

//
// LogModel
//
LogModel::LogModel(QObject *parent)
    : QAbstractListModel(parent)
{

}

int LogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return lst.count();
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= lst.size())
        return QVariant();

    if (role == Qt::DisplayRole)
		return lst.at(index.row())->getMessage();

	if (role == Qt::UserRole) {
        return lst.at(index.row())->getPriority();
    }
	if (role == Qt::UserRole + 1) {
        return lst.at(index.row())->getSource();
    }

	if (role == Qt::ForegroundRole)
		return lst.at(index.row())->priorityColor();

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

bool LogModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.row() >= 0 && index.row() < lst.size())
	{
		if (role == Qt::EditRole)
		{
			lst.value(index.row())->setMessage(value.toString());
			//lst.value(index.row())->setHighlight(isHighlight(value.toString()));
		}
		else if (role == Qt::UserRole) {
            lst.value(index.row())->setPriority(value.toString());
		}
		else if (role == Qt::UserRole + 1) {
            lst.value(index.row())->setSource(value.toString());
        }

        emit dataChanged(index, index);
        return true;
    }
    return false;
}

bool LogModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (count < 1 || row < 0 || row > rowCount(parent))
        return false;

    beginInsertRows(QModelIndex(), row, row + count - 1);

    for (int r = 0; r < count; ++r)
        lst.insert(row, std::make_shared<LogModelItem>());

    endInsertRows();

    return true;
}

/*!
    Removes \a count rows from the model, beginning at the given \a row.

    The \a parent index of the rows is optional and is only used for
    consistency with QAbstractItemModel. By default, a null index is
    specified, indicating that the rows are removed in the top level of
    the model.

    \sa QAbstractItemModel::removeRows()
*/

bool LogModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (count <= 0 || row < 0 || (row + count) > rowCount(parent))
        return false;

    beginRemoveRows(QModelIndex(), row, row + count - 1);

    for (int r = 0; r < count; ++r)
        lst.removeAt(row);

    endRemoveRows();

    return true;
}

Qt::DropActions LogModel::supportedDropActions() const
{
    return QAbstractItemModel::supportedDropActions() | Qt::MoveAction;
}
