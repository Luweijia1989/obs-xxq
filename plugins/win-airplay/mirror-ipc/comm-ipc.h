#pragma once
#include <QObject>

class MirrorRPCPrivate;
class MirrorRPC : public QObject {
	Q_OBJECT
public:
	MirrorRPC(QObject *parent = nullptr);
	void requestQuit();

signals:
	void quit();

private:
	MirrorRPCPrivate *m_private;
};
