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

	connect(&_server, SIGNAL(onJsonReceived(const ClientInfo&, quint8, const QJsonObject&)), this, SLOT(onJsonReceived(const ClientInfo&, quint8, const QJsonObject&)));
	connect(&_server, SIGNAL(onJsonError(const ClientInfo&, const QJsonParseError&)), this, SLOT(onJsonError(const ClientInfo&, const QJsonParseError&)));
	connect(&_server, SIGNAL(onError(const QTcpSocket&, const QString&)), this, SLOT(onError(const QTcpSocket&, const QString&)));

	if (!_server.startServer()) {
		QMessageBox::critical(this, tr("Server"),
			tr("Unable to start the server: %1.")
			.arg(_server.errorString()));
		close();
		return;
	}
}

void MainWindow::onJsonReceived(const ClientInfo& clientInfo, quint8 cmd, const QJsonObject &jsonData)
{
	switch (cmd)
	{
	case CMD_LOG:
		onCmdLog(clientInfo, jsonData);
		break;
	//default:
		//internalLog(QString("Unknown command: %1").arg(cmd), "ERROR", socketSource(clientInfo));
	}
}

void MainWindow::onJsonError(const ClientInfo& clientInfo, const QJsonParseError &error)
{
	//internalLog(QString("JSON parse error: %1").arg(error.errorString()), "ERROR", socketSource(clientInfo));
}

void MainWindow::onError(const QTcpSocket &clientSocket, const QString &error)
{
	//internalLog(QString("Error: %1").arg(error), "ERROR", socketSource(clientSocket, ""));
}

void MainWindow::onCmdLog(const ClientInfo& clientInfo, const QJsonObject &jsonData)
{
	QString f_source;
	if (jsonData.contains("source")) f_source = jsonData.value("source").toString();

	//internalProcessProtocol(socketSource(clientInfo), f_source, jsonData);
}
