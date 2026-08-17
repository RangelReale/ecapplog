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

// How much of the source a row draws. Neither field is bounded at the sending end, and sizeHint's
// width is what the horizontal scrollbar is sized to - one multi-kilobyte line would leave that
// scrollbar useless for every other row in the view. Both are whole in the detail window, one
// double-click away, and DisplayRole still carries the full message, so "Copy to clipboard" is
// unaffected. The message's own limit is the column below, not a count.
#define SOURCE_MAX_CHARS	200

// Where the source field starts, counted in characters of the log font from the start of the
// message, and the boundary the message is held inside - so the source starts at the same x on
// every row of a view and nothing can ever be drawn across it.
//
// It comes in three widths, and each log view takes the narrowest that fits the longest message in
// its own model, so a tab of one-line status messages keeps its source right beside them while a
// tab of verbose lines spaces out only itself. The widest is also the point past which a message is
// elided: beyond it a view would need a column further right than the window, which is no more
// readable than no column at all, and a line that long is one you open rather than read in the list.
#define MESSAGE_COLUMN_NARROW	60
#define MESSAGE_COLUMN_NORMAL	80
#define MESSAGE_COLUMN_WIDE		110

// The step this view sits at. Read off the model, not remembered here: the delegate is asked to
// measure and to paint at different moments, and a value held in the delegate would let the two
// disagree about where the column is.
static int messageColumnChars(const QModelIndex &index)
{
    const LogModel *model = qobject_cast<const LogModel*>(index.model());
    // character counts, not pixels: a message of 80 wide capitals is wider than 80 average
    // characters and overruns the column, which is the one case the column is allowed to lose
    const int longest = model ? model->maxMessageLength() : 0;

    if (longest <= MESSAGE_COLUMN_NARROW) return MESSAGE_COLUMN_NARROW;
    if (longest <= MESSAGE_COLUMN_NORMAL) return MESSAGE_COLUMN_NORMAL;

    return MESSAGE_COLUMN_WIDE;
}

// The source as a row draws it. simplified() first, then the cut, the same order DetailWindow uses
// for a tab tooltip: a row is one line of font height, so the newlines in a formatted payload would
// otherwise be drawn past the bottom of it and clipped.
static QString elideSource(const QString &source)
{
    const QString oneLine = source.simplified();
    if (oneLine.length() <= SOURCE_MAX_CHARS) return oneLine;

    return oneLine.left(SOURCE_MAX_CHARS) + QChar(0x2026);
}

// Separation between the message and the source, and the one thing the message's own width is short
// of the column. Font-relative, like every other measurement in here.
static int fieldGap(const QStyleOptionViewItem &option)
{
    return option.fontMetrics.horizontalAdvance("XX");
}

// Pixels from the start of the message to the start of the source. averageCharWidth, not a run of
// 'X' the way the fixed columns are measured: 'X' is half again wider than a typical character, so
// a column measured in 'X' would sit well to the right of where that many characters of real text
// end.
static int sourceColumn(const QStyleOptionViewItem &option, const QModelIndex &index)
{
    return option.fontMetrics.averageCharWidth() * messageColumnChars(index);
}

// The message as a row draws it. elidedText against the pixels the column actually has, not a count
// of characters: how many fit depends on the glyphs, and 110 capitals are half again wider than 110
// average characters - a count would let a line of them cross into the source.
//
// Deliberately not simplified() the way the source is. A run of spaces in a message is usually the
// sending program lining its own output up in columns, and collapsing those would change how every
// existing line reads for the sake of the few that are multi-line.
static QString elideMessage(const QStyleOptionViewItem &option, const QModelIndex &index,
    const QString &message)
{
    return option.fontMetrics.elidedText(message, Qt::ElideRight,
        sourceColumn(option, index) - fieldGap(option));
}

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
    QString rawMessage = index.data(MODELROLE_MESSAGE).toString();
    QString rawSource = index.data(MODELROLE_SOURCE).toString();
    QString message = elideMessage(option, index, rawMessage);
    QString source = elideSource(rawSource);

    if (option.state & QStyle::State_Selected) {
        painter->fillRect(rect, option.palette.brush(cg, QPalette::Highlight));
        painter->setPen(option.palette.color(cg, QPalette::HighlightedText));
    } else if (_highlight && (_highlight->matches(rawMessage) || _highlight->matches(rawSource))) {
        // Only the background changes: the pen keeps the priority colour the view put into
        // QPalette::Text from the item's ForegroundRole. A selected line is deliberately left to
        // the branch above, so a selection never reads as a highlight or the other way round.
        //
        // Matched against the raw text of both fields rather than the elided ones, so the
        // background keeps meaning "the word is in this entry somewhere" - the same thing it
        // already means for a message too long to fit the viewport. The bold runs below come from
        // whatever is drawn, so they always sit under the glyphs they belong to.
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

    // message, now a column of its own like the fields above it. elideMessage already fits the text
    // inside, so the narrowed rect is only there for the one thing it could not measure: a bold
    // highlight run, which elidedText sized with the plain font. Clipping that is the right way to
    // lose - a word cut short beats every source in the view shifting right for one row.
    const int column = sourceColumn(option, index);
    QRect rectMessage = drawRect.adjusted(0, 0, (column - fieldGap(option)) - drawRect.width(), 0);
    drawHighlighted(painter, rectMessage, message);
    drawRect.adjust(column, 0, 0, 0);

    // source
    if (!source.isEmpty())
    {
        drawHighlighted(painter, drawRect, source);
    }
}

// Draws text with every highlighted word in bold. Falls back to a single drawText when nothing
// matches, which is the usual case.
void LogDelegate::drawHighlighted(QPainter *painter, const QRect &rect, const QString &text) const
{
    QVector<QPair<int, int> > ranges;
    if (_highlight) ranges = _highlight->matchRanges(text);

    if (ranges.isEmpty())
    {
        painter->drawText(rect, Qt::AlignLeft, text);
        return;
    }

    QFont plainFont(painter->font());
    QFont boldFont(plainFont);
    boldFont.setBold(true);

    int x = rect.left();
    int at = 0;

    // walks the text as alternating plain and bold runs
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
        drawRun(text.mid(at, range.first - at), false);
        drawRun(text.mid(range.first, range.second), true);
        at = range.first + range.second;
        if (x > rect.right()) break;	// the rest is outside the column
    }
    drawRun(text.mid(at), false);

    painter->setFont(plainFont);
}

// The width drawHighlighted needs, so the horizontal scrollbar accounts for the wider bold runs.
int LogDelegate::highlightedWidth(const QFont &font, const QString &text) const
{
    QFontMetrics metrics(font);

    QVector<QPair<int, int> > ranges;
    if (_highlight) ranges = _highlight->matchRanges(text);

    if (ranges.isEmpty()) return metrics.horizontalAdvance(text);

    QFont boldFont(font);
    boldFont.setBold(true);
    QFontMetrics boldMetrics(boldFont);

    int width = 0;
    int at = 0;
    for (const QPair<int, int> &range : ranges)
    {
        width += metrics.horizontalAdvance(text.mid(at, range.first - at));
        width += boldMetrics.horizontalAdvance(text.mid(range.first, range.second));
        at = range.first + range.second;
    }
    width += metrics.horizontalAdvance(text.mid(at));

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
    // The same column and the same elided text customDrawDisplay draws, or the scrollbar stops short
    // of the source. A row with no source is measured on its message alone: the column is where a
    // source is *drawn*, not width every row is padded out to. qMin because customDrawDisplay clips
    // the message to the column, so pixels past it are never on screen to be scrolled to.
    const QString source = elideSource(index.data(MODELROLE_SOURCE).toString());
    const int column = sourceColumn(option, index);

    if (source.isEmpty())
    {
        width += qMin(column - fieldGap(option), highlightedWidth(option.font,
            elideMessage(option, index, index.data(MODELROLE_MESSAGE).toString())));
    }
    else
    {
        width += column + highlightedWidth(option.font, source);
    }

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
