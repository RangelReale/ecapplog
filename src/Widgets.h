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
};