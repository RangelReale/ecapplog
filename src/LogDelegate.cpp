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

// How much of the source a row draws. The field is not bounded at the sending end and a formatted
// payload is usually multi-line, while a row is one line of font height - so the newlines would be
// drawn past the bottom of it and clipped. It is whole in the detail window, one double-click
// away, and MODELROLE_SOURCE still carries all of it, so "Copy source to clipboard" and Find are
// unaffected.
#define SOURCE_MAX_CHARS	200

// The source as a row draws it. simplified() first, then the cut, the same order DetailWindow uses
// for a tab tooltip.
static QString elideSource(const QString &source)
{
    const QString oneLine = source.simplified();
    if (oneLine.length() <= SOURCE_MAX_CHARS) return oneLine;

    return oneLine.left(SOURCE_MAX_CHARS) + QChar(0x2026);
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
    QString text = index.data(Qt::DisplayRole).toString();

    // Deliberately only the source, not the message. A run of spaces in a message is usually the
    // sending program lining its own output up, and collapsing those would change how every
    // existing line reads for the sake of the few that are multi-line.
    if (index.column() == LOGCOL_SOURCE) text = elideSource(text);

    return option.fontMetrics.elidedText(text, Qt::ElideRight, width);
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
// header - so this only feeds the initial widths and "resize to contents" on a divider.
QSize LogDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // setOptions again: the alt columns are italic, and measuring them with the upright font would
    // size their section slightly short.
    QStyleOptionViewItem opt = setOptions(index, option);

    QString text = index.data(Qt::DisplayRole).toString();
    if (index.column() == LOGCOL_SOURCE) text = elideSource(text);

    return QSize(highlightedWidth(opt.font, text) + (cellMargin() * 2), opt.fontMetrics.height());
}
