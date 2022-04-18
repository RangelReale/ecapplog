#include "Server.h"
#include "Config.h"

#include <QDataStream>
#include <QJsonDocument>

//
// ApplicationInfo
//

ApplicationInfo::ApplicationInfo(QTcpSocket* socket) : _socket(socket), _hasbanner(false), _name()
{

}

ApplicationInfo::~ApplicationInfo()
{
	delete _socket;
}

QTcpSocket* ApplicationInfo::socket() const
{
	return _socket;
}

bool ApplicationInfo::hasBanner() const
{
	return _hasbanner;
}

void ApplicationInfo::setHasBanner()
{
	_hasbanner = true;
}

QString ApplicationInfo::getName() const
{
	return _name;
}

void ApplicationInfo::setName(const QString& name)
{
	_name = name;
}

//
// Server
//

Server::Server(QObject *parent)
	: QTcpServer(parent), _clientlist(), _seq()
{
	_worker = new ServerWorker;
	connect(_worker, &ServerWorker::onJsonReceived, this, &Server::onJsonReceived);
	connect(_worker, &ServerWorker::onJsonError, this, &Server::onJsonError);
	_worker->moveToThread(&_workerthread);
	_workerthread.start();
}

Server::~Server()
{
	_workerthread.quit();
	_workerthread.wait();
	delete _worker;
}

bool Server::startServer()
{
	int port = 13991;

	if (!this->listen(QHostAddress::Any, port))
	{
		return false;
	}
	else
	{
		//qDebug() << "Listening to " << serverAddress() << ":" << serverPort() << "...";
		return true;
	}
}

void Server::incomingConnection(qintptr socketDescriptor)
{
	ApplicationInfo *ci = new ApplicationInfo(new QTcpSocket);

	connect(ci->socket(), &QTcpSocket::readyRead, this, &Server::onReadyRead);
	connect(ci->socket(), &QTcpSocket::disconnected, this, &Server::onDisconnected);
	_clientlist.append(ci);

	if (!ci->socket()->setSocketDescriptor(socketDescriptor)) {
		QString appName(applicationName(*ci->socket(), ""));
		emit onError(appName, ci->socket()->errorString());
		return;
	}
}

void Server::onReadyRead()
{
	QTcpSocket *clientSocket = qobject_cast<QTcpSocket *>(QObject::sender());

	int idx = indexOf(clientSocket);
	if (idx == -1) {
		QString appName(applicationName(*clientSocket, ""));
		emit onError(appName, "Could not find client connection in internal list, disconnecting");
		clientSocket->close();
		return;
	}

	ApplicationInfo* appInfo = _clientlist.at(idx);

	QDataStream in(clientSocket);
	in.setVersion(QDataStream::Qt_5_5);

	while (true) {
		in.startTransaction();
		quint8 cmd;
		quint32 size;
		QByteArray data;
		in >> cmd; // read command
		in >> size; // try to read packet atomically
		if (size > 0) {
			data.resize(size);
			in.readRawData(data.data(), size);
		}

		if (!in.commitTransaction()) {
			return;     // wait for more data
		}

		QString appName(applicationName(*appInfo));

		if (cmd == CMD_BANNER)
		{
			if (appInfo->hasBanner() || !data.startsWith("ECAPPLOG "))
			{
				emit onError(appName, "Invalid banner received, disconnecting");
				clientSocket->close();
				return;
			}
			data.remove(0, 9);

			QString connname = QString::fromUtf8(data);
			if (!connname.isEmpty()) {
				appInfo->setName(QString("%1:%2").arg(connname).arg(++_seq));
			}
			appInfo->setHasBanner();
		} else {
			// parse json in worker thread
			QMetaObject::invokeMethod(_worker, "parseJson", Qt::QueuedConnection,
				Q_ARG(QString, appName), Q_ARG(quint8, cmd),
				Q_ARG(QByteArray, data));
		}
	}
}

void Server::onDisconnected()
{
	QTcpSocket *clientSocket = qobject_cast<QTcpSocket *>(QObject::sender());

	clientSocket->close();

	int idx = indexOf(clientSocket);
	if (idx != -1) {
		_clientlist.at(idx)->deleteLater();
		_clientlist.removeAt(idx);
	}
}

int Server::indexOf(QTcpSocket *clientSocket)
{
	for (int i = 0; i < _clientlist.length(); ++i) {
		if (_clientlist.at(i)->socket() == clientSocket) return i;
	}
	return -1;
}

QString Server::applicationName(const ApplicationInfo& appInfo)
{
	return applicationName(*appInfo.socket(), appInfo.getName());
}

QString Server::applicationName(const QTcpSocket& clientSocket, const QString& connname)
{
	if (connname.isEmpty())
		return QString("%1:%2").arg(clientSocket.peerAddress().toString()).arg(clientSocket.peerPort());
	return connname;
}

//
// ServerWorker
//

void ServerWorker::parseJson(const QString& appName, quint8 cmd, const QByteArray& data)
{
	QJsonParseError parseError;
	const QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
	if (parseError.error == QJsonParseError::NoError) {
		if (jsonDoc.isObject()) {
			emit onJsonReceived(appName, cmd, jsonDoc.object());
		}
	}
	else
	{
		emit onJsonError(appName, parseError);
	}
}
