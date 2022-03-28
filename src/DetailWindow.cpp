#include "DetailWindow.h"

#include <QIcon>
#include <QVBoxLayout>

DetailWindow::DetailWindow(QWidget *parent, QString message, QString source) :
	QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);

	setWindowTitle("Detail");
	setWindowIcon(QIcon(":/ecapplog.png"));

	_tabs = new QTabWidget(this);
	_tabs->setTabsClosable(false);

	if (!source.isEmpty()) {
		_source = new QTextEdit;
		_source->setText(source);
		_source->setMinimumWidth(600);
        _source->setMinimumHeight(450);
		_tabs->addTab(_source, "Source");
	}

	_message = new QTextEdit;
	_message->setText(message);
	_message->setMinimumWidth(600);
    _message->setMinimumHeight(450);
	_tabs->addTab(_message, "Text");

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(_tabs);

	setLayout(layout);
}
