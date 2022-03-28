//
// MainWindow.h
//

#pragma once

#define NOMINMAX

#include "Server.h"
#include "Data.h"

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

	void onJsonReceived(const ApplicationInfo &appInfo, quint8 cmd, const QJsonObject &jsonData);
	void onJsonError(const ApplicationInfo& appInfo, const QJsonParseError &error);
	void onError(const QTcpSocket &clientSocket, const QString &error);
private:
	QString applicationName(const ApplicationInfo& appInfo);
	QString applicationName(const QTcpSocket &clientSocket, const QString& connname);

	void onCmdLog(const ApplicationInfo& appInfo, const QJsonObject &jsonData);

	Server _server;
	Data _data;

	static MainWindow *self;
	ads::CDockManager* _dockManager;
};
