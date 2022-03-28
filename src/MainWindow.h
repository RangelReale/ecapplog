//
// MainWindow.h
//

#pragma once

#define NOMINMAX

#include "Server.h"

#include "DockManager.h"

#include <QListView>
#include <QMainWindow>
#include <QMenu>
#include <QAction>

#include <string>
#include <map>


class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	MainWindow(QWidget *parent = 0);

    static MainWindow *instance() { return self; }
public Q_SLOTS:

	void onJsonReceived(const ClientInfo &clientInfo, quint8 cmd, const QJsonObject &jsonData);
	void onJsonError(const ClientInfo& clientInfo, const QJsonParseError &error);
	void onError(const QTcpSocket &clientSocket, const QString &error);
private:
	void onCmdLog(const ClientInfo& clientInfo, const QJsonObject &jsonData);

	Server _server;

	static MainWindow *self;
	ads::CDockManager* _dockManager;
};
