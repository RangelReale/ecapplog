//
// FindDialog.h
//

#pragma once

#define NOMINMAX

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>

class FindDialog : public QDialog
{
	Q_OBJECT
public:
	FindDialog(QWidget *parent, const QString &text, bool caseSensitive);

	QString text() const;
	bool caseSensitive() const;
private:
	QLineEdit *_text;
	QCheckBox *_caseSensitive;
};
