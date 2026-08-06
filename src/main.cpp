#include "MainWindow.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setOrganizationName("RangelReale");
	app.setOrganizationDomain("rangelreale.com");
	app.setApplicationName("ECAppLog");
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
