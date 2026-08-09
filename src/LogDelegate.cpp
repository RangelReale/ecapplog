#include "LogDelegate.h"
#include "LogModel.h"
#include "Config.h"
#include "Highlight.h"

#include <QPainter>
#include <QStyle>
#include <QApplication>
#include <QFontMetrics>

// Background of a line holding one of the Edit -> Highlight words. It has to stay light: the
// message text keeps its priority colour (see LogModelItem::calcPriorityColor), and those colours
// are chosen to read against a light background.
#define HIGHLIGHT_BG_COLOR	QColor(255, 236, 160)

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
    QString message = index.data(MODELROLE_MESSAGE).toString();

    if (option.state & QStyle::State_Selected) {
        painter->fillRect(rect, option.palette.brush(cg, QPalette::Highlight));
        painter->setPen(option.palette.color(cg, QPalette::HighlightedText));
    } else if (_highlight && _highlight->matches(message)) {
        // Only the background changes: the pen keeps the priority colour the view put into
        // QPalette::Text from the item's ForegroundRole. A selected line is deliberately left to
        // the branch above, so a selection never reads as a highlight or the other way round.
        painter->fillRect(rect, HIGHLIGHT_BG_COLOR);
        painter->setPen(option.palette.color(cg, QPalette::Text));
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

    // app
    if (!altApp.isEmpty())
    {
        int pixelsApp = qMax(150, option.fontMetrics.horizontalAdvance(QString("XX[%1]XX").arg(altApp)));
        QString textApp = QString(" {%1} ").arg(altApp);
        QRect rectApp = drawRect.adjusted(0, 0, pixelsApp - drawRect.width(), 0);
        QFont oldFont(painter->font());
        QFont newFont(painter->font());
        newFont.setItalic(true);
        painter->setFont(newFont);
        painter->drawText(rectApp, Qt::AlignCenter, textApp);
        painter->setFont(oldFont);
        drawRect.adjust(pixelsApp, 0, 0, 0);
    }

    // category
    if (!altCategory.isEmpty())
    {
        int pixelsCat = qMax(150, option.fontMetrics.horizontalAdvance(QString("XX{%1}XX").arg(altCategory)));
        QString textCat = QString(" {%1} ").arg(altCategory);
        QRect rectCat = drawRect.adjusted(0, 0, pixelsCat - drawRect.width(), 0);
        QFont oldFont(painter->font());
        QFont newFont(painter->font());
        newFont.setItalic(true);
        painter->setFont(newFont);
        painter->drawText(rectCat, Qt::AlignCenter, textCat);
        painter->setFont(oldFont);
        drawRect.adjust(pixelsCat, 0, 0, 0);
    }

    // message
    drawMessage(painter, drawRect, message);
}

// Draws the message with every highlighted word in bold. Falls back to a single drawText when
// nothing matches, which is the usual case.
void LogDelegate::drawMessage(QPainter *painter, const QRect &rect, const QString &message) const
{
    QVector<QPair<int, int> > ranges;
    if (_highlight) ranges = _highlight->matchRanges(message);

    if (ranges.isEmpty())
    {
        painter->drawText(rect, Qt::AlignLeft, message);
        return;
    }

    QFont plainFont(painter->font());
    QFont boldFont(plainFont);
    boldFont.setBold(true);

    int x = rect.left();
    int at = 0;

    // walks the message as alternating plain and bold runs
    auto drawRun = [&](const QString &run, bool bold) {
        if (run.isEmpty()) return;
        painter->setFont(bold ? boldFont : plainFont);
        QRect runRect(rect);
        runRect.setLeft(x);
        painter->drawText(runRect, Qt::AlignLeft, run);
        x += QFontMetrics(painter->font()).horizontalAdvance(run);
    };

    for (const QPair<int, int> &range : ranges)
    {
        drawRun(message.mid(at, range.first - at), false);
        drawRun(message.mid(range.first, range.second), true);
        at = range.first + range.second;
        if (x > rect.right()) break;	// the rest is outside the column
    }
    drawRun(message.mid(at), false);

    painter->setFont(plainFont);
}

// The width drawMessage needs, so the horizontal scrollbar accounts for the wider bold runs.
int LogDelegate::messageWidth(const QFont &font, const QString &message) const
{
    QFontMetrics metrics(font);

    QVector<QPair<int, int> > ranges;
    if (_highlight) ranges = _highlight->matchRanges(message);

    if (ranges.isEmpty()) return metrics.horizontalAdvance(message);

    QFont boldFont(font);
    boldFont.setBold(true);
    QFontMetrics boldMetrics(boldFont);

    int width = 0;
    int at = 0;
    for (const QPair<int, int> &range : ranges)
    {
        width += metrics.horizontalAdvance(message.mid(at, range.first - at));
        width += boldMetrics.horizontalAdvance(message.mid(range.first, range.second));
        at = range.first + range.second;
    }
    width += metrics.horizontalAdvance(message.mid(at));

    return width;
}

QSize LogDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    //QSize size = QItemDelegate::sizeHint(option, index);
    QSize size(0, option.fontMetrics.height());

    QString altApp = index.data(MODELROLE_ALTAPP).toString();
    QString altCategory = index.data(MODELROLE_ALTCATEGORY).toString();

    const int frameHMargin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
    int width = (frameHMargin * 2) + 100;

    width += option.fontMetrics.horizontalAdvance("XXXXXXXXXXXXXXXXXXXXX");
    width += option.fontMetrics.horizontalAdvance(QString("X[%1]X").arg(Priority::PRIO_INFORMATION));
    width += messageWidth(option.font, index.data(MODELROLE_MESSAGE).toString());

    if (!altApp.isEmpty())
    {
        width += qMax(150, option.fontMetrics.horizontalAdvance(QString("XX[%1]XX").arg(altApp)));
    }

    if (!altCategory.isEmpty())
    {
        width += qMax(150, option.fontMetrics.horizontalAdvance(QString("XX{%1}XX").arg(altCategory)));
    }

    size.setWidth(width);
    return size;
}
