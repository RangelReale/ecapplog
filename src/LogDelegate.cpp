#include "LogDelegate.h"
#include "LogModel.h"
#include "Config.h"

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

    QRect drawRect = rect;

    // time
    int pixelsTime = option.fontMetrics.horizontalAdvance("XXXXXXXXXXXXXXXXXXXXX");
    //QString textTime = index.data(MODELROLE_TIME).toDateTime().toLocalTime().toString(Qt::ISODateWithMs);
    QString textTime = index.data(MODELROLE_TIME).toDateTime().toLocalTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QRect rectTime = drawRect.adjusted(0, 0, pixelsTime - drawRect.width(), 0);
    painter->drawText(rectTime, Qt::AlignCenter, textTime);
    drawRect.adjust(pixelsTime, 0, 0, 0);

    // priority
    int pixelsPrio = option.fontMetrics.horizontalAdvance(QString("X[%1]X").arg(Priority::PRIO_INFORMATION));
    QString textPrio = QString("[%1]").arg(index.data(MODELROLE_PRIORITY).toString());
    QRect rectPrio = drawRect.adjusted(0, 0, pixelsPrio - drawRect.width(), 0);
    painter->drawText(rectPrio, Qt::AlignCenter, textPrio);
    drawRect.adjust(pixelsPrio, 0, 0, 0);

    // message
    painter->drawText(drawRect, Qt::AlignLeft, index.data(MODELROLE_MESSAGE).toString());
}

