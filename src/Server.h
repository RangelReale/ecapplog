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
// Server
//
class Server : public QTcpServer
{
	Q_OBJECT
public:
	Server(QObject *parent = nullptr);

	bool startServer();
private slots:
	void onReadyRead();
	void onDisconnected();
signals:
	void onJsonReceived(const ApplicationInfo& appInfo, quint8 cmd, const QJsonObject& jsonData);
	void onJsonError(const ApplicationInfo& appInfo, const QJsonParseError &error);
	void onError(const QTcpSocket &clientSocket, const QString &error);
protected:
	void incomingConnection(qintptr socketDescriptor) override;
private:
	int indexOf(QTcpSocket *clientSocket);

	QList<ApplicationInfo*> _clientlist;
	QAtomicInteger<quint32> _seq;
};
