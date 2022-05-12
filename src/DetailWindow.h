//
// DetailWindow.h
//

#pragma once

#define NOMINMAX

#include <QTextEdit>
#include <QDialog>
#include <QTabWidget>
#include <QJsonDocument>

class DetailWindow : public QDialog
{
	Q_OBJECT
public:
	DetailWindow(QWidget *parent, QString message, QString source);
public Q_SLOTS:
	//void copyToClipboard();
	void textEditContextMenu(const QPoint &point);
private:
	void tryFormatJSON(const QString &text, QTextEdit *output);
	QJsonDocument tryParseJSON(const QString &text);

	QTabWidget *_tabs;
	QTextEdit *_message;
	QTextEdit *_source;
};
