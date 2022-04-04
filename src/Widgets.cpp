#include "Widgets.h"

#include <QTabBar>
#include <QEvent>
#include <QMouseEvent>

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
	return QTabWidget::eventFilter(o, e);    
}
