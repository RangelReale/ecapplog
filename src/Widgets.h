//
// Widgets.h
//

#pragma once

#define NOMINMAX

#include <QTabWidget>

class TabWidget : public QTabWidget
{
    Q_OBJECT
public:
    TabWidget(QWidget *parent = nullptr);
    bool eventFilter(QObject *o, QEvent *e);

private:
    // Pans the tab strip by |steps| tabs, negative to the left. Returns whether
    // anything actually moved.
    bool scrollTabBar(int steps);

    // Accumulated wheel travel not yet spent on a step, in eighths of a degree.
    int _wheelDelta = 0;
};