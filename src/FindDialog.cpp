#include "FindDialog.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QDialogButtonBox>

FindDialog::FindDialog(QWidget *parent, const QString &text, bool caseSensitive) :
	QDialog(parent)
{
	setWindowTitle("Find");

	_text = new QLineEdit;
	_text->setText(text);
	_text->setMinimumWidth(300);
	// so typing straight away replaces the previous search instead of appending to it
	_text->selectAll();

	_caseSensitive = new QCheckBox("&Case sensitive");
	_caseSensitive->setChecked(caseSensitive);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(new QLabel("Text:"));
	layout->addWidget(_text);
	layout->addWidget(_caseSensitive);
	layout->addWidget(buttons);

	setLayout(layout);
}

QString FindDialog::text() const
{
	return _text->text();
}

bool FindDialog::caseSensitive() const
{
	return _caseSensitive->isChecked();
}
