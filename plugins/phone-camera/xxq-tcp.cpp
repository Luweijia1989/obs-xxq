#include "xxq-tcp.h"
#include <qdebug.h>
#include <iostream>

#include <event2/event.h>
#include <event2/thread.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>

EventBase::EventBase()
{
	qRegisterMetaType<intptr_t>("intptr_t");

	WSADATA winsockInfo;
	WSAStartup(MAKEWORD(2, 2), &winsockInfo);

	evthread_use_windows_threads();
	auto config = event_config_new();
	m_eventBase = event_base_new_with_config(config);
	event_config_free(config);

	m_eventThread = std::thread(eventLoop, this);
}

EventBase::~EventBase()
{
	if (m_eventThread.joinable()) {
		event_base_loopbreak(m_eventBase);
		m_eventThread.join();
	}
}

void EventBase::eventLoop(EventBase *e)
{
	event_base_loop(e->m_eventBase, EVLOOP_NO_EXIT_ON_EMPTY);

	std::cout << __FILE__ << __FUNCTION__ << "end event loop";
}

TcpClient::TcpClient(QObject *parent) : QObject(parent), base(EventBase::getInstance()->eventBase())
{
	for (size_t i = 0; i < 3; i++) {
		conditions.append(new QWaitCondition);
	}
}

TcpClient::~TcpClient()
{
	disconnectFromHost(100);
	qDeleteAll(conditions);
	conditions.clear();
}

void TcpClient::customEventCallback(evutil_socket_t fd, short what, void *arg)
{
	Q_UNUSED(fd)
	Q_UNUSED(what)
	Event *e = (Event *)arg;
	TcpClient *client = (TcpClient *)e->user_data;
	if (e->type == (int)Type::Connect) {
		client->connectInternal();
	} else if (e->type == (int)Type::Disconnect) {
		client->disconnectInternal();
	} else if (e->type == (int)Type::Write) {
		if (client->m_bev && e->buffer && e->size)
			bufferevent_write(client->m_bev, e->buffer, e->size);
	}

	event_free(e->e);
	if (e->buffer)
		free(e->buffer);
	delete e;
}

bool TcpClient::eventInternal(int type, int timeout, const char *data, int size)
{
	auto e = new Event;
	e->user_data = this;
	e->type = type;
	e->block = timeout != 0;
	if (data && size) {
		e->buffer = (char *)malloc(size);
		memcpy(e->buffer, data, size);
		e->size = size;
	}

	e->e = event_new(base, -1, 0, customEventCallback, e);
	event_active(e->e, 0, 0);

	bool res = true;
	if (timeout != 0) {
		mutex.lock();
		res = conditions[type]->wait(&mutex, timeout);
		mutex.unlock();
	}

	return res;
}

void TcpClient::wake(Type type)
{
	mutex.lock();
	conditions[(int)type]->notify_one();
	mutex.unlock();
}

void TcpClient::eventCallback(struct bufferevent *bev, short events, void *ptr)
{
	TcpClient *c = (TcpClient *)ptr;
	if (events & BEV_EVENT_CONNECTED) {
		c->wake(Type::Connect);
		emit c->connected();
	} else if (events & BEV_EVENT_TIMEOUT) {

	} else {
		c->disconnectInternal();
		emit c->disconnected();
	}
}
void TcpClient::readCallback(struct bufferevent *bev, void *ctx)
{
	TcpClient *client = (TcpClient *)ctx;
	struct evbuffer *buf = bufferevent_get_input(client->m_bev);
	uint8_t readbuf[4096] = {0};
	int read = 0;

	while ((read = evbuffer_remove(buf, &readbuf, sizeof(readbuf))) > 0) {
		emit client->onData(readbuf, read);
	}
}

void TcpClient::writeCallback(struct bufferevent *bev, void *ctx)
{
	Q_UNUSED(bev)
	TcpClient *client = (TcpClient *)ctx;
	client->wake(Type::Write);
}

void TcpClient::connectInternal()
{
	m_bev = bufferevent_socket_new(EventBase::getInstance()->eventBase(), -1, 0);

	bufferevent_setcb(m_bev, readCallback, writeCallback, eventCallback, this);
	bufferevent_enable(m_bev, EV_READ | EV_WRITE);

	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	inet_pton(AF_INET, ip.toUtf8().data(), &sin.sin_addr.s_addr);

	if (bufferevent_socket_connect(m_bev, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		qDebug() << "connect to host: " << ip << " error";
	}
}

bool TcpClient::connectToHost(const char *ip, int port, uint32_t timeout)
{
	this->ip = ip;
	this->port = port;
	return eventInternal((int)Type::Connect, timeout);
}

void TcpClient::disconnectInternal()
{
	if (m_bev) {
		EVUTIL_CLOSESOCKET(bufferevent_getfd(m_bev));
		bufferevent_free(m_bev);
		m_bev = nullptr;
	}

	wake(Type::Disconnect);
}

void TcpClient::disconnectFromHost(uint32_t timeout)
{
	eventInternal((int)Type::Disconnect, timeout);
}

bool TcpClient::write(char *data, int size, uint32_t timeout)
{
	return eventInternal((int)Type::Write, timeout, data, size);
}

inline Connection::Connection(evutil_socket_t fd, bufferevent *bev, TcpServer *server)
{
	this->bev = bev;
	this->fd = fd;
	this->server = server;
	printf("Created connection with server ref: %p\n", server);
}

inline void Connection::send(const char *data, size_t numBytes)
{
	if (bufferevent_write(bev, data, numBytes) == -1) {
		printf("Error while sending in Connection::send()\n");
	}
}

inline void Connection::onRead(char *data, size_t size)
{
	QMutexLocker locker(&server->listener_mutex);
	if (server->event_listener)
		server->event_listener->onClientData(fd, data, size);
}
#include <qtimer.h>
TcpServer::TcpServer(QObject *parent) : QObject(parent), base(EventBase::getInstance()->eventBase()), listener(NULL)
{
	for (size_t i = 0; i < 3; i++) {
		conditions.append(new QWaitCondition);
	}

	QTimer *t = new QTimer;
	connect(t, &QTimer::timeout, this, [=](){
		char *s = "hello";
		sendToAllClients(s, strlen(s));
	});
	t->start(10);
}

TcpServer::~TcpServer()
{
	stop();

	qDeleteAll(conditions);
	conditions.clear();
}

bool TcpServer::start(int port)
{
	listen_port = port;
	listen_res = false;
	eventInternal((int)Type::Listen, -1);

	return listen_res;
}

void TcpServer::stop(int timeout)
{
	eventInternal((int)Type::Close, timeout);
}

int TcpServer::port()
{
	return listen_port;
}

void TcpServer::sendToAllClients(const char *data, int len, int timeout)
{
	eventInternal((int)Type::Write, timeout, data, len);
}

void TcpServer::sendTo(evutil_socket_t fd, const char *data, int len, int timeout)
{
	eventInternal((int)Type::Write, timeout, data, len, fd);
}

void TcpServer::setSocketEventListener(SocketEventListener *listener)
{
	QMutexLocker locker(&listener_mutex);
	event_listener = listener;
}

// ------------------------------------

bool TcpServer::eventInternal(int type, int timeout, const char *data, int size, evutil_socket_t fd)
{
	auto e = new Event;
	e->user_data = this;
	e->type = type;
	e->block = timeout != 0;
	if (data && size) {
		e->buffer = (char *)malloc(size);
		memcpy(e->buffer, data, size);
		e->size = size;
		e->fd = fd;
	}
	e->e = event_new(base, -1, 0, customEventCallback, e);
	event_active(e->e, 0, 0);

	bool res = true;
	if (timeout != 0) {
		mutex.lock();
		res = conditions[type]->wait(&mutex, timeout);
		mutex.unlock();
	}

	return res;
}

void TcpServer::wake(Type type)
{
	mutex.lock();
	conditions[(int)type]->notify_one();
	mutex.unlock();
}

void TcpServer::customEventCallback(evutil_socket_t fd, short what, void *arg)
{
	Q_UNUSED(fd)
	Q_UNUSED(what)
	Event *e = (Event *)arg;
	TcpServer *server = (TcpServer *)e->user_data;
	if (e->type == (int)Type::Listen) {
		server->listenInternal();
	} else if (e->type == (int)Type::Close) {
		server->stopInternal();
	} else if (e->type == (int)Type::Write) {
		auto &connections = server->connections;
		if (e->fd == 0) {
			auto it = connections.begin();
			while (it != connections.end()) {
				it.value()->send(e->buffer, e->size);
				++it;
			}
		} else {
			if (connections.contains(e->fd)) {
				connections[e->fd]->send(e->buffer, e->size);
			}
		}
	}

	event_free(e->e);
	if (e->buffer)
		free(e->buffer);
	delete e;
}

void TcpServer::listenInternal()
{
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	inet_pton(AF_INET, "0.0.0.0", &sin.sin_addr.s_addr);
	sin.sin_port = htons(listen_port);

	listener = evconnlistener_new_bind(base, TcpServer::listenerCallback, (void *)this, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
					   (struct sockaddr *)&sin, sizeof(sin));

	if (!listener) {
		printf("Cannot create listener.\n");
		wake(Type::Listen);
		return;
	}

	struct sockaddr_in addr;
	int len = sizeof(addr);
	getsockname(evconnlistener_get_fd(listener), (struct sockaddr *)&addr, &len);
	listen_port = ntohs(addr.sin_port);
	listen_res = true;
	wake(Type::Listen);
}

void TcpServer::stopInternal()
{
	for (auto iter = connections.begin(); iter != connections.end(); iter++) {
		Connection *conn = iter.value();
		EVUTIL_CLOSESOCKET(conn->fd);
		bufferevent_free(conn->bev);
		delete conn;
	}

	connections.clear();

	if (listener) {
		evconnlistener_free(listener);
		listener = nullptr;
	}

	wake(Type::Close);
}

void TcpServer::listenerCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *saddr, int socklen, void *data)
{
	TcpServer *server = static_cast<TcpServer *>(data);

	{
		QMutexLocker locker(&server->listener_mutex);
		if (server->event_listener && !server->event_listener->onNewConnection(fd)) {
			EVUTIL_CLOSESOCKET(fd);
			return;
		}
	}

	auto bev = bufferevent_socket_new(server->base, fd, 0);
	if (!bev) {
		printf("Error constructing bufferevent!\n");
		return;
	}

	Connection *conn = new Connection(fd, bev, server);
	bufferevent_setcb(bev, TcpServer::readCallback, TcpServer::writeCallback, TcpServer::eventCallback, (void *)conn);
	bufferevent_enable(bev, EV_WRITE);
	bufferevent_enable(bev, EV_READ);

	server->connections.insert(fd, conn);
}

void TcpServer::writeCallback(struct bufferevent *bev, void *data)
{
	struct evbuffer *output = bufferevent_get_output(bev);
	if (evbuffer_get_length(output) == 0) {
	}
	Connection *conn = static_cast<Connection *>(data);
	conn->server->wake(Type::Write);
}

void TcpServer::readCallback(struct bufferevent *bev, void *connection)
{
	Connection *conn = static_cast<Connection *>(connection);
	struct evbuffer *buf = bufferevent_get_input(bev);
	char readbuf[4096] = {0};
	size_t read = 0;

	while ((read = evbuffer_remove(buf, &readbuf, sizeof(readbuf))) > 0) {
		conn->onRead(readbuf, read);
	}
}

void TcpServer::eventCallback(struct bufferevent *bev, short events, void *data)
{
	Connection *conn = static_cast<Connection *>(data);
	TcpServer *server = static_cast<TcpServer *>(conn->server);

	auto fd = conn->fd;
	if (!(events & BEV_EVENT_TIMEOUT) || (!events & BEV_EVENT_CONNECTED)) {
		{
			auto &connections = server->connections;
			if (connections.contains(fd)) {
				Connection *conn = connections[fd];
				EVUTIL_CLOSESOCKET(fd);
				bufferevent_free(conn->bev);
				connections.remove(fd);
				delete conn;
			}
		}

		QMutexLocker locker(&server->listener_mutex);
		if (server->event_listener)
			server->event_listener->onConnectionLost(fd);
	} else {
		printf("unhandled.\n");
	}
}
