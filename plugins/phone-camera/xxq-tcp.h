#pragma once

#include <thread>
#include <qobject.h>
#include <qmap.h>

#include <qmutex.h>
#include <qwaitcondition.h>

#ifdef _WIN32
#define evutil_socket_t intptr_t
#else
#define evutil_socket_t int
#endif

struct event_base;
struct bufferevent;
struct evconnlistener;

class EventBase {
public:
	static EventBase *getInstance()
	{
		static EventBase base;
		return &base;
	}

	struct event_base *eventBase() { return m_eventBase; }

private:
	EventBase();
	~EventBase();

	static void eventLoop(EventBase *e);

private:
	struct event_base *m_eventBase = nullptr;
	std::thread m_eventThread;
};

struct Event {
	int type = -1;
	bool block = false;
	evutil_socket_t fd = 0;
	char *buffer = nullptr;
	int size = 0;
	struct event *e = nullptr;
	void *user_data = nullptr;
};

class TcpClient : public QObject {
	Q_OBJECT
public:
	enum class Type {
		Connect,
		Disconnect,
		Write,
	};

	TcpClient(QObject *parent = nullptr);
	~TcpClient();
	bool connectToHost(const char *ip, int port, uint32_t timeout);
	void disconnectFromHost(uint32_t timeout);
	bool write(char *data, int size, uint32_t timeout);

private:
	void connectInternal();
	void disconnectInternal();
	bool eventInternal(int type, int timeout, const char *data = nullptr, int size = 0);
	void wake(Type type);

signals:
	void connected();
	void disconnected();
	void onData(uint8_t *data, int size);

private:
	static void eventCallback(struct bufferevent *bev, short events, void *ptr);
	static void readCallback(struct bufferevent *bev, void *ctx);
	static void writeCallback(struct bufferevent *bev, void *ctx);
	static void customEventCallback(evutil_socket_t fd, short what, void *arg);

private:
	struct event_base *base = nullptr;
	struct bufferevent *m_bev = nullptr;
	QString ip;
	int port;
	QMutex mutex;
	QVector<QWaitCondition *> conditions;
};

class TcpServer;
class Connection {
public:
	Connection(evutil_socket_t fd, struct bufferevent *bev, TcpServer *server);
	void send(const char *data, size_t numBytes);
	void onRead(char *data, size_t size);

	struct bufferevent *bev;
	evutil_socket_t fd;
	TcpServer *server;
};

class SocketEventListener {
public:
	virtual void onClientData(evutil_socket_t fd, char *data, int size) = 0;
	virtual bool onNewConnection(evutil_socket_t fd) = 0;
	virtual void onConnectionLost(evutil_socket_t fd) = 0;
};

class TcpServer : public QObject {
	friend class Connection;
	Q_OBJECT
public:
	enum class Type {
		Listen,
		Close,
		Write,
	};
	TcpServer(QObject *parent = nullptr);
	~TcpServer();

	bool start(int port = 0);
	void stop(int timeout = 100);
	int port();
	void sendToAllClients(const char *data, int len, int timeout = 0);
	void sendTo(evutil_socket_t fd, const char *data, int len, int timeout = 0);
	void setSocketEventListener(SocketEventListener *listener);

private:
	void listenInternal();
	void stopInternal();
	bool eventInternal(int type, int timeout, const char *data = nullptr, int size = 0, evutil_socket_t fd = 0);
	void wake(Type type);

private:
	static void listenerCallback(struct evconnlistener *listener, evutil_socket_t socket, struct sockaddr *saddr, int socklen, void *server);
	static void writeCallback(struct bufferevent *, void *server);
	static void readCallback(struct bufferevent *, void *connection);
	static void eventCallback(struct bufferevent *, short, void *server);
	static void customEventCallback(evutil_socket_t fd, short what, void *arg);

private:
	struct event_base *base;
	struct evconnlistener *listener;
	int listen_port;
	bool listen_res = false;

	QMutex mutex;
	QVector<QWaitCondition *> conditions;

	QMap<evutil_socket_t, Connection *> connections;

	SocketEventListener *event_listener = nullptr;
	QMutex listener_mutex;
};
