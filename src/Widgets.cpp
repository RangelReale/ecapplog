#include "Widgets.h"

#include <QTabBar>
#include <QAction>
#include <QEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>

// One mouse wheel notch, in the eighths of a degree that QWheelEvent::angleDelta
// reports. One notch pans the strip by one tab.
#define WHEEL_STEP 120

TabWidget::TabWidget(QWidget *parent) : QTabWidget(parent)
{
    tabBar()->installEventFilter(this);
}

// Once the strip overflows it scrolls, and whatever sits off the end is easy to forget
// is there at all - with a category per log source there can be a lot of it. The corner
// button always shows how many tabs exist, and drops down the whole list so one can be
// reached without panning the strip to find it.
void TabWidget::enableTabList()
{
    if (_tabList)
        return;

    // Framed rather than flat: the strip already has the red per-tab count badges in
    // it, and a bare number next to those reads as one more badge instead of
    // something to click.
    _tabList = new QToolButton(this);
    _tabList->setPopupMode(QToolButton::InstantPopup);
    _tabList->setToolButtonStyle(Qt::ToolButtonTextOnly);

    // Categories appear and disappear as logs arrive, so the list is built when it is
    // asked for rather than kept in sync with the strip.
    QMenu *menu = new QMenu(_tabList);
    connect(menu, &QMenu::aboutToShow, this, &TabWidget::buildTabListMenu);
    _tabList->setMenu(menu);

    setCornerWidget(_tabList, Qt::TopRightCorner);

    // setCornerWidget only shows what it is handed if the tab widget is itself
    // visible, and here it is still being constructed, so the button has to be shown
    // by hand or it never appears at all.
    _tabList->show();

    updateTabList();
}

void TabWidget::tabInserted(int index)
{
	QTabWidget::tabInserted(index);
	updateTabList();
}

void TabWidget::tabRemoved(int index)
{
	QTabWidget::tabRemoved(index);
	updateTabList();
}

void TabWidget::updateTabList()
{
	if (!_tabList)
		return;

	const int tabs = count();
	_tabList->setText(QString::number(tabs));
	_tabList->setToolTip(tabs == 1 ? QString("1 tab") : QString("%1 tabs").arg(tabs));

	// An empty strip does happen: Edit > Clear empties a window, and a window created
	// to receive an application moved into it starts out with nothing. Disable the
	// button rather than hide it, so the width available to the tabs stays put instead
	// of the strip reflowing every time a window empties and fills again.
	_tabList->setEnabled(tabs > 0);
}

void TabWidget::buildTabListMenu()
{
	QMenu *menu = _tabList->menu();
	menu->clear();

	for (int i = 0; i < count(); ++i)
	{
		// The text is an application or category name, and & is a mnemonic marker in
		// menu text, so it has to be doubled the way refreshHighlightMenu does.
		QString label(tabText(i));
		label.replace("&", "&&");

		QAction *action = menu->addAction(label);
		action->setCheckable(true);
		action->setChecked(i == currentIndex());

		// A category can arrive or be closed while the menu sits open, which shifts
		// the indexes out from under it, so the tab is looked up again by its page
		// when the action actually fires. The QPointer covers the page having been
		// destroyed in the meantime, which leaves the entry doing nothing.
		QPointer<QWidget> page(widget(i));
		connect(action, &QAction::triggered, this, [this, page] {
			const int index = page ? indexOf(page) : -1;
			if (index >= 0)
				setCurrentIndex(index);
		});
	}
}

bool TabWidget::eventFilter(QObject *o, QEvent *e)
{
	if (o == tabBar() && e->type() == QEvent::MouseButtonPress) {
		QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(e);
        if (mouseEvent->button() == Qt::MiddleButton) {
            emit tabCloseRequested(tabBar()->tabAt(mouseEvent->pos()));
            return true;
        }
	}
	if (o == tabBar() && e->type() == QEvent::Wheel) {
		QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(e);

		// Whichever axis dominates drives the strip, so a horizontal two-finger
		// swipe pans it just as a vertical one does.
		const QPoint angle = wheelEvent->angleDelta();
		const int delta = qAbs(angle.x()) > qAbs(angle.y()) ? angle.x() : angle.y();
		if (delta == 0)
			return true;

		// A trackpad gesture arrives as a burst of small deltas, so spend them a
		// notch at a time and carry the remainder instead of stepping once per
		// event — otherwise a single swipe flies across the whole strip. A new
		// gesture, or a reversal mid-gesture, starts the count over so the change
		// of direction is not damped by travel left over from the old one.
		if (wheelEvent->phase() == Qt::ScrollBegin || (delta < 0) != (_wheelDelta < 0))
			_wheelDelta = 0;
		_wheelDelta += delta;

		// Negative delta moves forward through the strip, matching the convention
		// ADS uses for its own dock tab bar (CDockAreaTabBar::wheelEvent).
		scrollTabBar(-(_wheelDelta / WHEEL_STEP));
		_wheelDelta %= WHEEL_STEP;

		// Always consume it, even when nothing moved. On Windows and Linux
		// QTabBar::wheelEvent is not the no-op it is on macOS — it would change the
		// current tab, and this is meant to pan the strip without disturbing the
		// selection on every platform.
		return true;
	}
	return QTabWidget::eventFilter(o, e);
}

bool TabWidget::scrollTabBar(int steps)
{
	if (steps == 0)
		return false;

	// QTabBar keeps its scroll offset in QTabBarPrivate with no public accessor, and
	// makeVisible() is private as well, so the only way to pan the strip from outside
	// is the pair of arrow buttons the style puts at either end of it: clicking one
	// runs QTabBarPrivate::_q_scrollTabs(), which advances by exactly one tab.
	//
	// Qt 5 gives those buttons no objectName, so they have to be found by type. That
	// is unambiguous here even though both strips are setTabsClosable(true) and the
	// category tabs carry the count badge, because Qt's close button is a private
	// QAbstractButton subclass and the badge is a QLabel — the arrows are the only
	// QToolButton children of the tab bar. Should a future Qt lay this out
	// differently we find something other than a pair and do nothing, which costs the
	// wheel gesture and nothing else.
	const auto buttons = tabBar()->findChildren<QToolButton *>(QString(), Qt::FindDirectChildrenOnly);
	if (buttons.size() != 2)
		return false;

	// Both tab bars face north, so the buttons are ordered by their x position.
	QToolButton *left = buttons.at(0);
	QToolButton *right = buttons.at(1);
	if (left->x() > right->x())
		std::swap(left, right);

	// Qt disables the arrow at whichever end the strip has already reached, and
	// QAbstractButton::click() does nothing on a disabled button, so running off
	// either end needs no bounds checking here. Both arrows are disabled while every
	// tab fits, which is what makes the wheel a no-op on a strip with no overflow.
	QToolButton *button = steps < 0 ? left : right;
	bool scrolled = false;
	for (int i = 0; i < qAbs(steps) && button->isEnabled(); ++i) {
		button->click();
		scrolled = true;
	}
	return scrolled;
}
