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

    QString altApp = index.data(MODELROLE_ALTAPP).toString();
    QString altCategory = index.data(MODELROLE_ALTCATEGORY).toString();

    // time
    int pixelsTime = option.fontMetrics.horizontalAdvance("XXXXXXXXXXXXXXXXXXXXX");
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

    // category
    if (!altCategory.isEmpty())
    {
        int pixelsCat = option.fontMetrics.horizontalAdvance(QString("XX{%1}XX").arg(altCategory));
        QString textCat = QString(" {%1} ").arg(altCategory);
        QRect rectCat = drawRect.adjusted(0, 0, pixelsCat - drawRect.width(), 0);
        painter->drawText(rectCat, Qt::AlignCenter, textCat);
        drawRect.adjust(pixelsCat, 0, 0, 0);
    }

    // message
    painter->drawText(drawRect, Qt::AlignLeft, index.data(MODELROLE_MESSAGE).toString());
}

QSize LogDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize size = QItemDelegate::sizeHint(option, index);

    QString altApp = index.data(MODELROLE_ALTAPP).toString();
    QString altCategory = index.data(MODELROLE_ALTCATEGORY).toString();

    const int frameHMargin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
    int width = (frameHMargin * 2) + 100;

    width += option.fontMetrics.horizontalAdvance("XXXXXXXXXXXXXXXXXXXXX");
    width += option.fontMetrics.horizontalAdvance(QString("X[%1]X").arg(Priority::PRIO_INFORMATION));
    width += option.fontMetrics.horizontalAdvance(index.data(MODELROLE_MESSAGE).toString());

    if (!altCategory.isEmpty())
    {
        width += option.fontMetrics.horizontalAdvance(QString("XX{%1}XX").arg(altCategory));
    }

    size.setWidth(width);
    return size;
}
