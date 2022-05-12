#include "DetailWindow.h"

#include <QIcon>
#include <QVBoxLayout>
#include <QMenu>
#include <QJsonDocument>
#include <QTextCursor>
#include <QRegExp>

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
		_source->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(_source, &QTextEdit::customContextMenuRequested, this, &DetailWindow::textEditContextMenu);
		_tabs->addTab(_source, "Source");
	}

	_message = new QTextEdit;
	_message->setText(message);
	_message->setMinimumWidth(600);
    _message->setMinimumHeight(450);
	_message->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(_message, &QTextEdit::customContextMenuRequested, this, &DetailWindow::textEditContextMenu);
	_tabs->addTab(_message, "Text");

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(_tabs);

	setLayout(layout);
}

void DetailWindow::textEditContextMenu(const QPoint &point)
{
	if (point.isNull())
		return;
 	
	QTextEdit *te = qobject_cast<QTextEdit*>(sender());

	QMenu *menu = te->createStandardContextMenu();
	QAction *parseJSON = nullptr;
	if (!te->textCursor().selectedText().isEmpty()) {
		menu->addSeparator();
    	parseJSON = menu->addAction("Try parsing &JSON");
	}
    QAction *selectedItem = menu->exec(te->mapToGlobal(point));
	if (selectedItem)
	{
		if (selectedItem == parseJSON)
		{
			tryFormatJSON(te->textCursor().selectedText(), te);
		}
	}
    delete menu;
}

void DetailWindow::tryFormatJSON(const QString &text, QTextEdit *output)
{
	QJsonDocument doc = tryParseJSON(text);
	if (doc.isNull()) return;

	output->moveCursor(QTextCursor::End);

	output->append("\n");
	output->append(QString("-").repeated(40));
	output->append("\n");
	output->append(doc.toJson(QJsonDocument::Indented));
}

QJsonDocument DetailWindow::tryParseJSON(const QString &text)
{
	QJsonDocument doc = QJsonDocument::fromJson(text.toLocal8Bit());
	if (!doc.isNull()) return doc;

	// try unquoting
	QString unquoted(text);
	unquoted.replace(QRegExp("\\\\(.)"), "\\1");

	doc = QJsonDocument::fromJson(unquoted.toLocal8Bit());
	if (!doc.isNull()) return doc;

	return QJsonDocument();
}