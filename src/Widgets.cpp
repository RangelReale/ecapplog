#include "Widgets.h"

#include <QTabBar>
#include <QEvent>
#include <QMouseEvent>
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
