//
// Data.h
//

# pragma once

#define NOMINMAX

#include <QAbstractListModel>

class LogModelItem
{
public:
	LogModelItem() {}

	const QString &getMessage() const { return _message; }
	const QString &getPriority() const { return _priority; }
	const QString &getSource() const { return _source; }

	void setMessage(const QString &message) { _message = message; }
	void setPriority(const QString &priority) { _priority = priority; _prioritycolor = calcPriorityColor(); }
	void setSource(const QString &source) { _source = source; }

	QColor priorityColor() const { return _prioritycolor; }
private:
	QColor calcPriorityColor() const;

	QString _message;
	QString _priority;
	QColor _prioritycolor;
	QString _source;
};

class LogModel : public QAbstractListModel
{
public:
    Q_OBJECT
public:
    explicit LogModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);

    Qt::ItemFlags flags(const QModelIndex &index) const;

    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex());
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());

    Qt::DropActions supportedDropActions() const;
private:
    Q_DISABLE_COPY(LogModel)
    QList<std::shared_ptr<LogModelItem> > lst;
};
