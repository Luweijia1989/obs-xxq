#pragma once

#include <qthread.h>
#include <qset.h>
#include <qmutex.h>
#include <qeventloop.h>

struct peer {
	int fd;
	char *ob_buf;
	uint32_t ob_size;
	uint32_t ob_capacity;
	char *ib_buf;
	uint32_t ib_size;
	uint32_t ib_capacity;
	short events;
};

class TcpClient : public QThread {
	Q_OBJECT
public:
	TcpClient(QObject *parent = nullptr);
	~TcpClient();
	bool connectToHost(QString ip, int port);
	void close();
	void send(char *data, int size);
	void waitForBytesWritten(uint32_t timeout = 1000);

private:
	int createSocket();
	void socketEvent(short events);
	void socketData();
	void socketWrite();
	void socketClose();

signals:
	void connected();
	void onData(char *data, int size);

protected:
	void run() override;

private:
	peer *m_peer = nullptr;
	bool m_shouldExit = false;
	QMutex m_dataLock;
	QEventLoop m_eventloop;
};

class TcpServer : public QThread {
	Q_OBJECT
public:
	TcpServer(QObject *parent = nullptr);
	virtual ~TcpServer();

	bool startServer(int port = 0);
	void stopServer();
	int port() { return m_port; }

	virtual void onClientData(int fd, char *data, int size)
	{
		Q_UNUSED(fd)
		Q_UNUSED(data)
		Q_UNUSED(size)
	}
	virtual bool onNewConnection(int fd)
	{
		Q_UNUSED(fd)
		return true;
	}
	virtual void onConnectionLost(int fd) { Q_UNUSED(fd) }

private:
	int createSocket();
	int clientAccept();
	void clientProcess(int fd, short events);
	void clientRecv(int fd);
	void clientClose(int fd);

protected:
	void run() override;

private:
	int m_listenFD = -1;
	int m_port = 0;
	bool m_shouldExit = false;
	QSet<int> m_clients;
};
