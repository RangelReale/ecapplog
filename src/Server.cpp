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

	connect(ci->socket(), SIGNAL(readyRead()), this, SLOT(onReadyRead()));
	connect(ci->socket(), SIGNAL(disconnected()), this, SLOT(onDisconnected()));
	_clientlist.append(ci);

	if (!ci->socket()->setSocketDescriptor(socketDescriptor)) {
		emit onError(*ci->socket(), ci->socket()->errorString());
		return;
	}
}

void Server::onReadyRead()
{
	QTcpSocket *clientSocket = qobject_cast<QTcpSocket *>(QObject::sender());

	int idx = indexOf(clientSocket);
	if (idx == -1) {
		emit onError(*clientSocket, "Could not find client connection in internal list, disconnecting");
		clientSocket->close();
		return;
	}

	ApplicationInfo* clientInfo = _clientlist.at(idx);

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

		if (cmd == CMD_BANNER)
		{
			if (clientInfo->hasBanner() || !data.startsWith("ECAPPLOG "))
			{
				emit onError(*clientSocket, "Invalid banner received, disconnecting");
				clientSocket->close();
				return;
			}
			data.remove(0, 9);

			QString connname = QString::fromUtf8(data);
			if (!connname.isEmpty()) {
				clientInfo->setName(QString("%1:%2").arg(connname).arg(++_seq));
			}
			clientInfo->setHasBanner();
		} else {
			QJsonParseError parseError;
			const QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
			if (parseError.error == QJsonParseError::NoError) {
				if (jsonDoc.isObject()) {
					emit onJsonReceived(*clientInfo, cmd, jsonDoc.object());
				}
			}
			else
			{
				emit onJsonError(*clientInfo, parseError);
			}
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
