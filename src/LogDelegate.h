//
// LogDelegate.h
//

#pragma once

#define NOMINMAX

#include <QItemDelegate>

class Highlight;

// Draws one cell of a log view. QItemDelegate rather than QStyledItemDelegate: paint() is built
// out of the protected setOptions, drawBackground and drawFocus, which only this base has.
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
    QString cellText(const QStyleOptionViewItem &option, const QModelIndex &index, int width) const;
    bool rowMatchesHighlight(const QModelIndex &index) const;
    // Both take any text, not just the message: the source field is drawn and measured through
    // them too, so it gets the same bold runs.
    void drawHighlighted(QPainter *painter, const QRect &rect, const QString &text) const;
    int highlightedWidth(const QFont &font, const QString &text) const;

    const Highlight *_highlight;
};
