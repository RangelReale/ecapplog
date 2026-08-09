//
// LogDelegate.h
//

#pragma once

#define NOMINMAX

#include <QItemDelegate>

class Highlight;

class LogDelegate : public QItemDelegate
{
public:
    // highlight may be null: the delegate then draws every line plainly
    explicit LogDelegate(const Highlight *highlight = nullptr) : _highlight(highlight) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
private:
    void customDrawDisplay(QPainter *painter, const QStyleOptionViewItem &option,
        const QRect &rect, const QModelIndex &index) const;
    void drawMessage(QPainter *painter, const QRect &rect, const QString &message) const;
    int messageWidth(const QFont &font, const QString &message) const;

    const Highlight *_highlight;
};
