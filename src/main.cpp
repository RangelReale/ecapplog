#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setOrganizationName("RangelReale");
	app.setOrganizationDomain("rangelreale.com");
	app.setApplicationName("ECAppLog");
	MainWindow mainwindow;
	mainwindow.show();

	return app.exec();
}