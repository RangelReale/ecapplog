//
// LogDelegate.h
//

#pragma once

#define NOMINMAX

#include <QItemDelegate>

class LogDelegate : public QItemDelegate
{
public:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
private:
    void customDrawDisplay(QPainter *painter, const QStyleOptionViewItem &option,
        const QRect &rect, const QModelIndex &index) const;
};
