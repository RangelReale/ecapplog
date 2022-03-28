#include "LogDelegate.h"

#include <QPainter>
#include <QStyle>
#include <QApplication>

void LogDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    //QItemDelegate::paint(painter, option, index);
    //painter->fillRect(option.rect.adjusted(1, 1, -1, -1), Qt::SolidPattern);

    Q_ASSERT(index.isValid());

    QStyleOptionViewItem opt = setOptions(index, option);

    // prepare
    painter->save();

    QStyle *style = QApplication::style();

    drawBackground(painter, opt, index);
    customDrawDisplay(painter, opt, opt.rect, index);
    //style->drawItemText(painter, option.rect, option.displayAlignment, QApplication::palette(), true, index.data(Qt::DisplayRole).toString());
    drawFocus(painter, opt, opt.rect);

    // done
    painter->restore();
}

void LogDelegate::customDrawDisplay(QPainter *painter, const QStyleOptionViewItem &option,
    const QRect &rect, const QModelIndex &index) const
{
    QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                              ? QPalette::Normal : QPalette::Disabled;
    if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
        cg = QPalette::Inactive;
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(rect, option.palette.brush(cg, QPalette::Highlight));
        painter->setPen(option.palette.color(cg, QPalette::HighlightedText));
    } else {
        painter->setPen(option.palette.color(cg, QPalette::Text));
    }

    // if (text.isEmpty())
    //     return;

    int pixelsWide = option.fontMetrics.horizontalAdvance("XXXXXXXXXXXXXXXXXXXXXXXX");

    QString text = index.data(Qt::DisplayRole).toString();

    painter->drawText(rect, option.displayAlignment, text);

    //QApplication::style()->drawItemText(painter, rect, option.displayAlignment, QApplication::palette(), true, text);
}

