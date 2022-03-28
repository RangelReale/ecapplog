//
// LogDelegate.h
//

#pragma once

#define NOMINMAX

#include <QItemDelegate>

class LogDelegate : public QItemDelegate
{
public:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
private:
    void customDrawDisplay(QPainter *painter, const QStyleOptionViewItem &option,
        const QRect &rect, const QModelIndex &index) const;
};
