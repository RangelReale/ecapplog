#include "MainWindow.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setOrganizationName("RangelReale");
	app.setOrganizationDomain("rangelreale.com");
	app.setApplicationName("ECAppLog");
    QFont font = app.font();
    font.setPointSize(16);
    app.setFont(font);
#ifdef Q_OS_DARWIN
	// Use "fusion" style on Mac to have scrollable tabs in QTabWidget
	app.setStyle(QStyleFactory::create("Fusion"));
#endif
	MainWindow mainwindow;
	mainwindow.show();

	return app.exec();
}
