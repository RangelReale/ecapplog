#include "DetailWindow.h"

#include "JsonHighlighter.h"
#include "JsonScan.h"

#include <QFontDatabase>
#include <QList>
#include <QMenu>
#include <QSplitter>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

DetailWindow::DetailWindow(QWidget *parent, const QString &text) :
	QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);

	setWindowTitle("Detail");

	_splitter = new QSplitter(this);
	_splitter->setOrientation(Qt::Vertical);
	_splitter->setChildrenCollapsible(false);

	_log = new QTextEdit;
	// setPlainText, not setText: setText sniffs for rich text, and a log line that happens to
	// contain angle brackets would come out with pieces missing.
	_log->setPlainText(text);
	_log->setReadOnly(true);
	_log->setMinimumWidth(600);
	_log->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(_log, &QTextEdit::customContextMenuRequested, this, &DetailWindow::textEditContextMenu);
	_splitter->addWidget(_log);

	// Always built, and hidden by syncJsonTabsVisible() while it has no tabs. Keeping it in
	// the splitter rather than adding and removing it means the context menu can drop a tab
	// in at any time without rebuilding the layout.
	_jsonTabs = new QTabWidget;
	_jsonTabs->setTabsClosable(true);
	_jsonTabs->setDocumentMode(true);
	connect(_jsonTabs, &QTabWidget::tabCloseRequested, this, &DetailWindow::jsonTabCloseRequested);
	_splitter->addWidget(_jsonTabs);

	int unnamed = 0;
	const QVector<JsonScan::Match> matches = JsonScan::findAll(text);
	for (const JsonScan::Match &match : matches)
	{
		// an unwrapped payload gets the key path it came from, which is far more use as a tab
		// label than its position in the line
		addJsonTab(match.path.isEmpty() ? QString("JSON %1").arg(++unnamed) : match.path, match.doc);
	}

	// The formatted JSON is what the window is for; the raw text above it only needs enough
	// room to find your place in.
	_splitter->setStretchFactor(0, 1);
	_splitter->setStretchFactor(1, 3);
	_splitter->setSizes(QList<int>{250, 400});

	syncJsonTabsVisible();

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(_splitter);

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
			// selectedText() writes line breaks as U+2029, which the parser rejects as a raw
			// control character inside a string - a multi-line selection never parsed until
			// this was undone.
			QString selection(te->textCursor().selectedText());
			selection.replace(QChar(0x2029), QChar('\n'));

			const QJsonDocument doc = JsonScan::tryParse(selection);
			if (!doc.isNull())
				_jsonTabs->setCurrentWidget(addJsonTab("Selection", doc));
		}
	}
	delete menu;
}

void DetailWindow::jsonTabCloseRequested(int index)
{
	QWidget *tab = _jsonTabs->widget(index);
	_jsonTabs->removeTab(index);
	delete tab;

	syncJsonTabsVisible();
}

QTextEdit *DetailWindow::addJsonTab(const QString &label, const QJsonDocument &doc)
{
	QTextEdit *edit = new QTextEdit;
	edit->setReadOnly(true);
	// indentation only lines up in a fixed-width font, and wrapping it defeats the point
	edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	edit->setLineWrapMode(QTextEdit::NoWrap);
	edit->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
	edit->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(edit, &QTextEdit::customContextMenuRequested, this, &DetailWindow::textEditContextMenu);

	// parented to the document, so it goes away with the tab
	new JsonHighlighter(edit->document(), edit->palette());

	const int index = _jsonTabs->addTab(edit, label);
	// a row of "JSON 1", "JSON 2" tabs says nothing about what is in them
	_jsonTabs->setTabToolTip(index, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)).left(200));

	syncJsonTabsVisible();

	return edit;
}

void DetailWindow::syncJsonTabsVisible()
{
	const bool visible = _jsonTabs->count() > 0;

	_jsonTabs->setVisible(visible);
	// With nothing below it the log pane should fill the window - this is also what gives the
	// dialog a usable default height when the entry holds no JSON at all.
	_log->setMinimumHeight(visible ? 0 : 450);
}
