//
// DetailWindow.h
//

#pragma once

#define NOMINMAX

#include <QDialog>
#include <QJsonDocument>
#include <QStringList>

class QSplitter;
class QTabWidget;
class QTextEdit;

class DetailWindow : public QDialog
{
	Q_OBJECT
public:
	// The sources arrive as a list rather than one joined string because this side is the one
	// that knows how to format JSON: each has to be indented or left raw on its own merits,
	// and the finished tab only gets a highlighter if at least one of them parsed. Joining
	// them in the caller would throw that away.
	DetailWindow(QWidget *parent, const QString &text, const QStringList &sources);
public Q_SLOTS:
	//void copyToClipboard();
	void textEditContextMenu(const QPoint &point);
private:
	QTextEdit *addTab(const QString &label, const QString &content, bool highlight, int index);
	QTextEdit *addJsonTab(const QString &label, const QJsonDocument &doc);
	void addSourceTab(const QStringList &sources);
	void syncJsonTabsVisible();

	QSplitter  *_splitter;
	QTextEdit  *_log;
	QTabWidget *_jsonTabs;
};
