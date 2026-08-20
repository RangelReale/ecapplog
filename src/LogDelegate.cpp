#include "LogDelegate.h"
#include "LogModel.h"
#include "Config.h"
#include "Highlight.h"

#include <QPainter>
#include <QStyle>
#include <QApplication>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QToolTip>
#include <QHelpEvent>
#include <QAbstractItemView>
#include <QWidget>

// Background of a line holding one of the Edit -> Highlight words. It has to stay light: the
// message text keeps its priority colour (see LogModelItem::calcPriorityColor), and those colours
// are chosen to read against a light background.
#define HIGHLIGHT_BG_COLOR	QColor(255, 236, 160)

// A field as one row's worth of text. Nothing is cut here: a column is resizable, so how much of a
// field is visible is the width the user gave it, and there is no length at which the rest of the
// value stops being theirs to look at. This only flattens - a row is one line of font height, and
// QPainter::drawText lays an embedded newline out as a further line, which AlignVCenter then
// centres as a block. That is what made a three-line message draw its middle line inside the row
// and clip the first and third away entirely.
//
// Flattening the string here rather than passing draw flags down means the elide, the highlight
// ranges, the tooltip test and sizeHint all measure the one string that reaches the screen.
static QString oneLine(const QString &text, int column)
{
    // The source is simplified() outright. It is usually a formatted payload whose indentation is
    // there to be read down the page, and on a single line it is nothing but runs of spaces.
    if (column == LOGCOL_SOURCE) return text.simplified();

    // A message only loses its line breaks. A run of spaces in a message is usually the sending
    // program lining its own output up, and collapsing those would change how every existing line
    // reads for the sake of the few that are multi-line.
    if (!text.contains(QLatin1Char('\n')) && !text.contains(QLatin1Char('\r'))) return text;

    // A run rather than one character at a time, so a CRLF - or a blank line inside a stack trace -
    // does not become two or three spaces where a reader expects one.
    static const QRegularExpression newlines(QStringLiteral("[\\r\\n]+"));

    QString flat(text);
    flat.replace(newlines, QStringLiteral(" "));
    return flat;
}

// Breathing room between a cell and its column dividers, matching what the style leaves around
// item text elsewhere.
static int cellMargin()
{
    return QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
}

void LogDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_ASSERT(index.isValid());

    // setOptions is what applies the model's FontRole, TextAlignmentRole and ForegroundRole to the
    // option, so the italic of the two alt columns, the centring of the fixed ones and the
    // priority colour all arrive through it rather than being special-cased below.
    QStyleOptionViewItem opt = setOptions(index, option);

    // prepare
    painter->save();

    drawBackground(painter, opt, index);
    customDrawDisplay(painter, opt, opt.rect, index);
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
    } else if (_highlight && rowMatchesHighlight(index)) {
        // Only the background changes: the pen keeps the priority colour the view put into
        // QPalette::Text from the item's ForegroundRole. A selected line is deliberately left to
        // the branch above, so a selection never reads as a highlight or the other way round.
        //
        // The test is on the whole entry rather than on this one cell, so every column of a
        // matching row fills and the band reads as one row the way it did when the row was a
        // single cell. The bold runs below come from whatever is drawn, so they always sit under
        // the glyphs they belong to.
        painter->fillRect(rect, HIGHLIGHT_BG_COLOR);
        painter->setPen(option.palette.color(cg, QPalette::Text));
    } else {
        painter->setPen(option.palette.color(cg, QPalette::Text));
    }

    // The font from the option, not the one the view left on the painter: the alt columns are
    // italic, and drawHighlighted derives its bold face from whatever it finds here.
    painter->setFont(option.font);

    const QRect textRect = rect.adjusted(cellMargin(), 0, -cellMargin(), 0);
    if (textRect.width() <= 0) return;

    const QString text = cellText(option, index, textRect.width());
    if (text.isEmpty()) return;

    if (index.column() == LOGCOL_MESSAGE || index.column() == LOGCOL_SOURCE)
    {
        drawHighlighted(painter, textRect, text);
    }
    else
    {
        // The four fixed columns are drawn plainly, as they were when the row was one cell: a
        // highlight word matching a priority or a redirected category name is not what the word
        // list is for, and drawHighlighted lays its runs out from the left, which a centred
        // column has no use for.
        painter->drawText(textRect, option.displayAlignment, text);
    }
}

// What one cell shows, elided to the pixels it has. Elided against the column width rather than a
// character count because how many characters fit depends on the glyphs - a run of capitals is
// half again wider than the same number of average characters.
QString LogDelegate::cellText(const QStyleOptionViewItem &option, const QModelIndex &index,
    int width) const
{
    const QString text = oneLine(index.data(Qt::DisplayRole).toString(), index.column());

    return elideHighlighted(option.fontMetrics, option.font, text, width);
}

// Elides to the width the line is actually going to be drawn at. QFontMetrics::elidedText measures
// every character in the plain face, but drawHighlighted puts the matched runs in bold - so a line
// holding a highlight word came out wider than the budget it was elided to, and the tail past the
// ellipsis was clipped mid-glyph against the column divider instead.
QString LogDelegate::elideHighlighted(const QFontMetrics &metrics, const QFont &font,
    const QString &text, int width) const
{
    // No highlight word in this line - which is the usual case, and the case where the plain
    // metrics are already exact.
    if (!_highlight || _highlight->matchRanges(text).isEmpty())
        return metrics.elidedText(text, Qt::ElideRight, width);

    if (highlightedWidth(font, text) <= width) return text;

    const QString ellipsis(QChar(0x2026));
    const int ellipsisWidth = metrics.horizontalAdvance(ellipsis);
    if (ellipsisWidth > width) return QString();

    // Binary search the longest prefix that fits once the ellipsis is on the end. A prefix's width
    // is not proportional to its length - a bold run inside it costs extra - so there is nothing to
    // solve for directly. The plain fit bounds the search from above: bold is the wider face, so
    // the bold-aware answer can never be the longer of the two.
    int lo = 0;
    int hi = qMax(0, metrics.elidedText(text, Qt::ElideRight, width).length() - 1);

    while (lo < hi)
    {
        const int mid = (lo + hi + 1) / 2;
        if (highlightedWidth(font, text.left(mid)) + ellipsisWidth <= width) lo = mid;
        else hi = mid - 1;
    }

    return text.left(lo) + ellipsis;
}

// Whether the entry this cell belongs to holds a highlight word anywhere. Read from the raw
// message and source rather than from what is drawn, so the band still means "the word is in this
// entry somewhere" for a field too long to fit its column.
bool LogDelegate::rowMatchesHighlight(const QModelIndex &index) const
{
    if (!_highlight) return false;

    return _highlight->matches(index.data(MODELROLE_MESSAGE).toString())
        || _highlight->matches(index.data(MODELROLE_SOURCE).toString());
}

// Draws text with every highlighted word in bold. Falls back to a single drawText when nothing
// matches, which is the usual case.
void LogDelegate::drawHighlighted(QPainter *painter, const QRect &rect, const QString &text) const
{
    QVector<QPair<int, int> > ranges;
    if (_highlight) ranges = _highlight->matchRanges(text);

    if (ranges.isEmpty())
    {
        painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, text);
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
        painter->drawText(runRect, Qt::AlignLeft | Qt::AlignVCenter, run);
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

// The width drawHighlighted needs, so that resizing a column to its contents accounts for the
// wider bold runs.
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

// What one cell would like to be. Unlike the single-cell row this replaced, nothing here decides
// where a field is drawn - the header owns that, and the horizontal scrollbar is sized from the
// header. The starting widths do not come through here either: applyLogColumns sizes a new header
// itself and never asks the delegate. So the width below is only ever "resize to contents" on a
// double-clicked divider, and the height is the row height, via setUniformRowHeights.
QSize LogDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // setOptions again: the alt columns are italic, and measuring them with the upright font would
    // size their section slightly short.
    QStyleOptionViewItem opt = setOptions(index, option);

    const QString text = oneLine(index.data(Qt::DisplayRole).toString(), index.column());
    int width = highlightedWidth(opt.font, text) + (cellMargin() * 2);

    // A ceiling on the ask, not a cut to the text. Since the source is no longer shortened to a
    // character count, the honest width of a cell holding a formatted payload can be tens of
    // thousands of pixels, and a double-click on the divider would hand back a section that far
    // out - which the user then has to drag all the way back, and which the saved layout would
    // remember for every tab opened afterwards. A column wider than the window it lives in shows
    // no more of a line than one the width of the window, so that is where the ask stops. Dragging
    // past it by hand is still allowed; nothing here clamps a width the user chose.
    if (option.widget && option.widget->window())
        width = qMin(width, option.widget->window()->width());

    return QSize(width, opt.fontMetrics.height());
}

// The full value behind a cell, shown only when the cell is too narrow to hold it. Without this an
// elided field has to be dragged wider or opened in the detail window to be read at all, and the
// source - the field most likely to outrun its column - is the one it is least convenient to open.
//
// Deliberately not a Qt::ToolTipRole on the model: whether a cell is elided is a question about the
// column width, which the model has no business knowing, and a tooltip on every cell the user
// happens to rest on would be noise on a list this dense.
bool LogDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view,
    const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (!event || !view || event->type() != QEvent::ToolTip || !index.isValid())
        return QItemDelegate::helpEvent(event, view, option, index);

    QStyleOptionViewItem opt = setOptions(index, option);

    // The same string, measured the same way, against the same rect paint uses - so the tooltip
    // appears exactly when there is an ellipsis on screen and not one row sooner.
    const QString text = oneLine(index.data(Qt::DisplayRole).toString(), index.column());
    const int available = option.rect.adjusted(cellMargin(), 0, -cellMargin(), 0).width();

    if (!text.isEmpty() && highlightedWidth(opt.font, text) > available)
    {
        // The raw value, not the flattened one: a tooltip is free to be several lines, and a
        // formatted payload is far easier to read with its own line breaks left in.
        QToolTip::showText(event->globalPos(), index.data(Qt::DisplayRole).toString(), view);
        return true;
    }

    QToolTip::hideText();
    return false;
}
