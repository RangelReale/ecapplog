#include "MainWindow.h"
#include "Config.h"

#include <QApplication>
#include <QStyle>
#include <QMenu>
#include <QMessageBox>
#include <QMenuBar>
#include <QAction>
#include <QInputDialog>
#include <QDateTime>
#include <QSettings>
#include <QVariant>
#include <QMetaType>

MainWindow *MainWindow::self;

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent)
{
	MainWindow::self = this;

    setGeometry(0, style()->pixelMetric(QStyle::PM_TitleBarHeight), 400, 400);

	setWindowTitle("ECAppLog");
	setWindowIcon(QIcon(":/ecapplog.png"));

	_dockManager = new ads::CDockManager(this);

	connect(&_server, SIGNAL(onJsonReceived(const Appinfo&, quint8, const QJsonObject&)), this, SLOT(onJsonReceived(const Appinfo&, quint8, const QJsonObject&)));
	connect(&_server, SIGNAL(onJsonError(const Appinfo&, const QJsonParseError&)), this, SLOT(onJsonError(const Appinfo&, const QJsonParseError&)));
	connect(&_server, SIGNAL(onError(const QTcpSocket&, const QString&)), this, SLOT(onError(const QTcpSocket&, const QString&)));

	if (!_server.startServer()) {
		QMessageBox::critical(this, tr("Server"),
			tr("Unable to start the server: %1.")
			.arg(_server.errorString()));
		close();
		return;
	}
}

QString MainWindow::applicationName(const ApplicationInfo& appInfo)
{
	return applicationName(*appInfo.socket(), appInfo.getName());
}

QString MainWindow::applicationName(const QTcpSocket &clientSocket, const QString& connname)
{
	if (connname.isEmpty())
		return QString("%1:%2").arg(clientSocket.peerAddress().toString()).arg(clientSocket.peerPort());
	return connname;
}

void MainWindow::onJsonReceived(const ApplicationInfo& appInfo, quint8 cmd, const QJsonObject &jsonData)
{
	switch (cmd)
	{
	case CMD_LOG:
		_data.log(applicationName(appInfo), jsonData);
		break;
	default:
		_data.log(applicationName(appInfo), QDateTime(), "ECAPPLOG", Priority::PRIO_ERROR, 
			QString("Unknown command: %1").arg(cmd));
	}
}

void MainWindow::onJsonError(const ApplicationInfo& appInfo, const QJsonParseError &error)
{
		_data.log(applicationName(appInfo), QDateTime(), "ECAPPLOG", Priority::PRIO_ERROR, 
			QString("JSON parse error: %1").arg(error.errorString()));
}

void MainWindow::onError(const QTcpSocket &clientSocket, const QString &error)
{
	_data.log(applicationName(clientSocket, ""), QDateTime(), "ECAPPLOG", Priority::PRIO_ERROR, 
		QString("Error: %1").arg(error));
}

void MainWindow::onCmdLog(const ApplicationInfo& appInfo, const QJsonObject &jsonData)
{
	_data.log(applicationName(appInfo), jsonData);
}
