#include "MainWindow.h"
#include "AppIcon.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setOrganizationName("RangelReale");
	app.setOrganizationDomain("rangelreale.com");
	app.setApplicationName("ECAppLog");
#ifndef Q_OS_DARWIN
	// Set once here rather than per window: QWidget::windowIcon() falls back to the application
	// icon, so this covers the main window, the dialogs, and every log view floated out into a
	// top-level window by the docking system - including any added later.
	//
	// Not on macOS, and the exclusion is load-bearing rather than a micro-optimisation. Qt maps
	// setWindowIcon onto NSApplication's applicationIconImage, which is the Dock tile - so this
	// call would *replace* the bundle's ecapplog.icns with the full-bleed artwork from the Qt
	// resource. Since Big Sur every Dock icon is drawn at 824 in a 1024 canvas, and the .icns is
	// the only one of the two that carries that margin; letting the resource win makes ecapplog
	// render visibly larger than every neighbouring icon. macOS loses nothing here: it takes the
	// Dock and Finder icon from the bundle, and does not draw a window icon in title bars at all.
	app.setWindowIcon(appIcon());
#endif
	// The font size is owned by MainWindow (see MainWindow::applyFontSize): it has to be applied
	// after setStyle() below and once widgets exist, neither of which is true here.
#ifdef Q_OS_DARWIN
	// Use "fusion" style on Mac to have scrollable tabs in QTabWidget
	app.setStyle(QStyleFactory::create("Fusion"));
#endif
	MainWindow mainwindow;
	mainwindow.show();

	return app.exec();
}
