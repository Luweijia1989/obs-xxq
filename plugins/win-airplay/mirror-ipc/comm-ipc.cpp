#include "comm-ipc.h"
#include "ipc-private.h"
#include <QJsonDocument>
#include <QJsonObject>

MirrorRPC::MirrorRPC(QObject *parent /*= nullptr*/) : QObject(parent)
{
	m_private = new MirrorRPCPrivate;
	m_private->setParent(this);
	connect(m_private, &MirrorRPCPrivate::commandReceived, this, [=](const QString &cmd){
		QJsonDocument jd = QJsonDocument::fromJson(cmd.toUtf8());
		auto obj = jd.object();
		QString type = obj["type"].toString();
		if (type == "quit")
			emit quit();
	});
}

void MirrorRPC::requestQuit()
{
	QJsonObject obj;
	obj["type"] = "quit";
	QJsonDocument jd(obj);
	m_private->sendCMD(jd.toJson(QJsonDocument::Compact));
}
