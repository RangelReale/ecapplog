#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setOrganizationName("ECAppLog");
	app.setOrganizationDomain("ecapplog.dev");
	app.setApplicationName("ECAppLog");
	MainWindow mainwindow;
	mainwindow.show();

	return app.exec();
}