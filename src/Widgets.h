//
// Widgets.h
//

#pragma once

#define NOMINMAX

#include <QTabWidget>

class QToolButton;

class TabWidget : public QTabWidget
{
    Q_OBJECT
public:
    TabWidget(QWidget *parent = nullptr);
    bool eventFilter(QObject *o, QEvent *e);

    // Adds the corner button that counts the tabs and lists them. Opt-in: it earns
    // its keep on the category strip, which routinely runs to dozens of tabs, and
    // only takes up room on the application strip, which rarely fills up.
    void enableTabList();

protected:
    void tabInserted(int index);
    void tabRemoved(int index);

private:
    // Pans the tab strip by |steps| tabs, negative to the left. Returns whether
    // anything actually moved.
    bool scrollTabBar(int steps);

    // Keeps the corner button's count in step with the strip.
    void updateTabList();

    // Fills the corner button's menu with the tabs as they stand right now.
    void buildTabListMenu();

    // Accumulated wheel travel not yet spent on a step, in eighths of a degree.
    int _wheelDelta = 0;

    // Corner button: how many tabs there are, and the list of them on click. Null
    // until enableTabList() is called, which not every strip does.
    QToolButton *_tabList = nullptr;
};
