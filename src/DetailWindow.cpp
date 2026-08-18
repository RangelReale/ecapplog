#include "DetailWindow.h"

#include "JsonFormat.h"
#include "JsonHighlighter.h"
#include "JsonScan.h"

#include <QApplication>
#include <QFontDatabase>
#include <QFontInfo>
#include <QList>
#include <QMenu>
#include <QSplitter>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

namespace
{
	// The fixed-pitch family, at the size the rest of the app is running at.
	//
	// The size has to be set, not inherited: QFontDatabase hands out the system's idea of a code
	// font, which is 11pt on macOS, while this app forces a 16pt default there - so the JSON came
	// out visibly smaller than the raw pane directly above it in the same window. Taking the
	// application font's size is also what puts these tabs under View -> Font size along with
	// everything else; only the family is the system's to choose.
	QFont jsonFont()
	{
		QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);

		// QFontInfo resolves the size in use, where QFont::pointSize() reports -1 for a platform
		// font specified in pixels - the same trap defaultFontSize() and menuViewFont() in
		// MainWindow already work around.
		font.setPointSize(QFontInfo(QApplication::font()).pointSize());

		return font;
	}
}

DetailWindow::DetailWindow(QWidget *parent, const QString &text, const QStringList &sources) :
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
	//
	// Not closable: every tab is a reading of the one entry this window was opened on, so closing
	// one throws away information that only comes back by closing the window and opening it again.
	_jsonTabs = new QTabWidget;
	_jsonTabs->setDocumentMode(true);
	_splitter->addWidget(_jsonTabs);

	// The source goes first, and so takes the tab the window opens on: it is the entry's payload,
	// which is what the window is usually opened to read. Everything after it appends, so the two
	// groups stay in the order they are built - SOURCE and its JSON, then the message's.
	addSourceTab(sources);

	// The source is not appended to the message text and is not scanned as part of it: it is almost
	// always the entry's payload, so scanning it here would put the same data on screen twice, once
	// as a JSON tab and once as SOURCE. addSourceTab scans it separately, on its own terms.
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

QTextEdit *DetailWindow::addTab(const QString &label, const QString &content, bool highlight, int index)
{
	QTextEdit *edit = new QTextEdit;
	edit->setReadOnly(true);
	// indentation only lines up in a fixed-width font, and wrapping it defeats the point
	edit->setFont(jsonFont());
	edit->setLineWrapMode(QTextEdit::NoWrap);
	edit->setPlainText(content);
	edit->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(edit, &QTextEdit::customContextMenuRequested, this, &DetailWindow::textEditContextMenu);

	if (highlight) {
		// parented to the document, so it goes away with the tab
		new JsonHighlighter(edit->document(), edit->palette());
	}

	const int at = _jsonTabs->insertTab(index, edit, label);
	// a row of "JSON 1", "JSON 2" tabs says nothing about what is in them
	_jsonTabs->setTabToolTip(at, content.simplified().left(200));

	syncJsonTabsVisible();

	return edit;
}

QTextEdit *DetailWindow::addJsonTab(const QString &label, const QJsonDocument &doc)
{
	// Appended. The source group is built first and never moves, so a tab made by hand later - a
	// "Selection" from the context menu - lands at the end, which is where the newest one belongs.
	return addTab(label, JsonFormat::indented(doc), true, -1);
}

void DetailWindow::addSourceTab(const QStringList &sources)
{
	if (sources.isEmpty()) return;

	QStringList formatted;
	QStringList unparsed;
	bool anyJson = false;

	for (const QString &source : sources)
	{
		const QJsonDocument doc = JsonScan::tryParse(source);
		if (doc.isNull()) {
			formatted.append(source);
			unparsed.append(source);
		} else {
			// trimmed because the formatted text ends in a newline, which would double up the blank
			// line the join puts between one selected entry's source and the next
			formatted.append(JsonFormat::indented(doc).trimmed());
			anyJson = true;
		}
	}

	addTab("SOURCE", formatted.join("\n\n"), anyJson, -1);

	// A source that parsed end to end is already on screen, formatted, in the tab above; only the
	// ones that did not are scanned, because a source is as likely as a message to be JSON carried
	// somewhere inside prose. They are scanned in one call rather than one each: JSONSCAN_MAX_MATCHES
	// is per call, and a multi-row selection would otherwise multiply it by the rows selected.
	int unnamed = 0;
	const QVector<JsonScan::Match> matches = JsonScan::findAll(unparsed.join("\n\n"));
	for (const JsonScan::Match &match : matches)
	{
		// Prefixed, and not left to addJsonTab: an unwrapped payload is labelled with its bare key
		// path, so "payload" from a source and "payload" from the message would be two tabs with
		// the same name and no way to tell which held which.
		addTab(match.path.isEmpty() ? QString("SOURCE JSON %1").arg(++unnamed)
				: QString("SOURCE %1").arg(match.path),
			JsonFormat::indented(match.doc), true, -1);
	}
}

void DetailWindow::changeEvent(QEvent *event)
{
	QDialog::changeEvent(event);

	// View -> Font size reaches the widgets already on screen as an ApplicationFontChange, and
	// the raw pane above picks it up by inheritance. The tabs cannot: they carry an explicit
	// font, which is the whole point of them, and an explicit font is exactly what that event
	// does not touch. Without this the two halves of one window drift apart in size.
	if (event->type() == QEvent::ApplicationFontChange)
	{
		const QFont font = jsonFont();
		for (int at = 0; at < _jsonTabs->count(); ++at)
			_jsonTabs->widget(at)->setFont(font);
	}
}

void DetailWindow::syncJsonTabsVisible()
{
	const bool visible = _jsonTabs->count() > 0;

	_jsonTabs->setVisible(visible);
	// With nothing below it the log pane should fill the window - this is also what gives the
	// dialog a usable default height when the entry holds no JSON at all.
	_log->setMinimumHeight(visible ? 0 : 450);
}
