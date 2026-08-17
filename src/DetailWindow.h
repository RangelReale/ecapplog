//
// DetailWindow.h
//

#pragma once

#define NOMINMAX

#include <QDialog>
#include <QJsonDocument>

class QSplitter;
class QTabWidget;
class QTextEdit;

class DetailWindow : public QDialog
{
	Q_OBJECT
public:
	DetailWindow(QWidget *parent, const QString &text);
public Q_SLOTS:
	//void copyToClipboard();
	void textEditContextMenu(const QPoint &point);
	void jsonTabCloseRequested(int index);
private:
	QTextEdit *addJsonTab(const QString &label, const QJsonDocument &doc);
	void syncJsonTabsVisible();

	QSplitter  *_splitter;
	QTextEdit  *_log;
	QTabWidget *_jsonTabs;
};
