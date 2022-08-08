#include "DetailWindow.h"

#include <QIcon>
#include <QSplitter>
#include <QVBoxLayout>
#include <QMenu>
#include <QList>
#include <QJsonDocument>
#include <QTextCursor>
#include <QRegExp>

DetailWindow::DetailWindow(QWidget *parent, QString message, QString source) :
	QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);

	setWindowTitle("Detail");
	setWindowIcon(QIcon(":/ecapplog.png"));

	QSplitter* splitter = new QSplitter(this);
	splitter->setOrientation(Qt::Vertical);
	splitter->setChildrenCollapsible(false);

	if (!source.isEmpty()) {
		_source = new QTextEdit;
		_source->setText(source);
		_source->setMinimumWidth(600);
		_source->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(_source, &QTextEdit::customContextMenuRequested, this, &DetailWindow::textEditContextMenu);
		splitter->addWidget(_source);
	}

	_message = new QTextEdit;
	_message->setText(message);
	_message->setMinimumWidth(600);
	if (source.isEmpty()) {
		_message->setMinimumHeight(450);
	}
	_message->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(_message, &QTextEdit::customContextMenuRequested, this, &DetailWindow::textEditContextMenu);
	splitter->addWidget(_message);

	if (!source.isEmpty()) {
		splitter->setStretchFactor(0, 3);
		splitter->setStretchFactor(1, 1);
		splitter->setSizes(QList<int>{500, 150});
	}

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(splitter);

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