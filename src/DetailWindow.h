//
// DetailWindow.h
//

#pragma once

#define NOMINMAX

#include <QTextEdit>
#include <QDialog>
#include <QTabWidget>

class DetailWindow : public QDialog
{
	Q_OBJECT
public:
	DetailWindow(QWidget *parent, QString message, QString source);
public Q_SLOTS:
	//void copyToClipboard();
private:
	QTabWidget *_tabs;
	QTextEdit *_message;
	QTextEdit *_source;
};
