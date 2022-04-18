//
// Server.h
//

#pragma once

#define NOMINMAX

#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QJsonObject>
#include <QJsonParseError>
#include <QAtomicInteger>
#include <QThread>


//
// ApplicationInfo
//
class ApplicationInfo : public QObject
{
	Q_OBJECT
public:
	ApplicationInfo(QTcpSocket* socket);
	~ApplicationInfo();

	QTcpSocket* socket() const;
	bool hasBanner() const;
	void setHasBanner();
	QString getName() const;
	void setName(const QString& name);
private:
	QTcpSocket* _socket;
	bool _hasbanner;
	QString _name;
};

//
// ServerWorker
//
class ServerWorker : public QObject
{
	Q_OBJECT
public slots:
	void parseJson(const QString& appName, quint8 cmd, const QByteArray& data);
signals:
	void onJsonReceived(const QString& appName, quint8 cmd, const QJsonObject& jsonData);
	void onJsonError(const QString& appName, const QJsonParseError& error);
};

//
// Server
//
class Server : public QTcpServer
{
	Q_OBJECT
public:
	Server(QObject *parent = nullptr);
	~Server();

	bool startServer();
private slots:
	void onReadyRead();
	void onDisconnected();
signals:
	void onJsonReceived(const QString& appName, quint8 cmd, const QJsonObject& jsonData);
	void onJsonError(const QString& appName, const QJsonParseError &error);
	void onError(const QString& appName, const QString &error);
protected:
	void incomingConnection(qintptr socketDescriptor) override;
	QString applicationName(const ApplicationInfo& appInfo);
	QString applicationName(const QTcpSocket& clientSocket, const QString& connname);
private:
	int indexOf(QTcpSocket *clientSocket);

	QList<ApplicationInfo*> _clientlist;
	QAtomicInteger<quint32> _seq;
	QThread _workerthread;
	ServerWorker* _worker;
};
