#include "xxq-tcp.h"
#include <WinSock2.h>
#include <Windows.h>
#include <qdebug.h>
#include <qtimer.h>

enum fdowner { FD_LISTEN, FD_CLIENT, FD_CONNECTED };

struct fdlist {
	int count;
	int capacity;
	enum fdowner *owners;
	struct pollfd *fds;
};

static void fdlist_create(struct fdlist *list)
{
	list->count = 0;
	list->capacity = 4;
	list->owners = (enum fdowner *)malloc(sizeof(*list->owners) * list->capacity);
	list->fds = (struct pollfd *)malloc(sizeof(*list->fds) * list->capacity);
}

static void fdlist_add(struct fdlist *list, enum fdowner owner, int fd, short events)
{
	if (list->count == list->capacity) {
		list->capacity *= 2;
		list->owners = (enum fdowner *)realloc(list->owners, sizeof(*list->owners) * list->capacity);
		list->fds = (struct pollfd *)realloc(list->fds, sizeof(*list->fds) * list->capacity);
	}
	list->owners[list->count] = owner;
	list->fds[list->count].fd = fd;
	list->fds[list->count].events = events;
	list->fds[list->count].revents = 0;
	list->count++;
}

static void fdlist_free(struct fdlist *list)
{
	list->count = 0;
	list->capacity = 0;
	free(list->owners);
	list->owners = NULL;
	free(list->fds);
	list->fds = NULL;
}

static void fdlist_reset(struct fdlist *list)
{
	list->count = 0;
}

TcpClient::TcpClient(QObject *parent) : QThread(parent), m_dataLock(QMutex::Recursive)
{
	connect(this, &TcpClient::disconnected, this, &TcpClient::close);
}

TcpClient::~TcpClient()
{
	close();
}

bool TcpClient::connectToHost(QString ip, int port)
{
	auto fd = createSocket();
	if (fd < 0)
		return false;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip.toUtf8().data());
	addr.sin_port = htons(port);
	auto res = ::connect(fd, (SOCKADDR *)&addr, sizeof(addr));
	if (res == SOCKET_ERROR) {
		auto err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK) {
			closesocket(fd);
			return false;
		}
	}

	m_peer = (struct peer *)malloc(sizeof(struct peer));
	memset(m_peer, 0, sizeof(struct peer));

	m_peer->fd = fd;
	m_peer->ob_buf = (char *)malloc(0x10000);
	m_peer->ob_size = 0;
	m_peer->ob_capacity = 0x10000;
	m_peer->ib_buf = (char *)malloc(0x10000);
	m_peer->ib_size = 0;
	m_peer->ib_capacity = 0x10000;
	m_peer->events = POLLIN;

	m_shouldExit = false;
	start();
	return true;
}

void TcpClient::close()
{
	m_shouldExit = true;
	wait();

	socketClose();
}

void TcpClient::send(char *data, int size)
{
	QMutexLocker locker(&m_dataLock);
	m_sendCache.append(data, size);
}

void TcpClient::waitForBytesWritten(uint32_t timeout)
{
	QTimer timer;
	timer.setSingleShot(true);
	connect(&timer, &QTimer::timeout, &m_eventloop, &QEventLoop::quit);
	timer.start(timeout);
	m_eventloop.exec();
}

int TcpClient::createSocket()
{
	WSADATA wsaData = {0};
	auto ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0) {
		qDebug() << "WSAStartup error";
		return -1;
	}

	SOCKET fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd == INVALID_SOCKET) {
		qDebug() << "create socket error: " << WSAGetLastError();
		return -1;
	}

	u_long iMode = 1;
	ret = ioctlsocket(fd, FIONBIO, &iMode);
	if (ret != NO_ERROR) {
		qDebug() << "set nonblock socket error: " << ret;
		return -1;
	}

	return fd;
}

void TcpClient::socketData()
{
	int res = recv(m_peer->fd, m_peer->ib_buf, m_peer->ib_capacity, 0);
	if (res <= 0) {
		if (res < 0)
			qDebug("Receive from client fd %d failed: %s", m_peer->fd, strerror(errno));
		else
			qDebug("Client %d connection closed", m_peer->fd);

		emit disconnected();
		return;
	}

	emit onData(m_peer->ib_buf, res);
}

void TcpClient::socketWrite()
{
	if (!m_peer->ob_size) {
		qDebug("Client %d OUT process but nothing to send?", m_peer->fd);
		m_peer->events &= ~POLLOUT;
		return;
	}
	auto res = ::send(m_peer->fd, m_peer->ob_buf, m_peer->ob_size, 0);
	if (res <= 0) {
		qDebug("Sending to client fd %d failed: %d %s", m_peer->fd, res, strerror(errno));
		emit disconnected();
		return;
	}
	if ((uint32_t)res == m_peer->ob_size) {
		m_peer->ob_size = 0;
		m_peer->events &= ~POLLOUT;
	} else {
		m_peer->ob_size -= res;
		memmove(m_peer->ob_buf, m_peer->ob_buf + res, m_peer->ob_size);
	}

	m_eventloop.quit();
}

void TcpClient::socketClose()
{
	if (!m_peer)
		return;

	closesocket(m_peer->fd);
	free(m_peer->ib_buf);
	free(m_peer->ob_buf);
	free(m_peer);
	m_peer = nullptr;
	m_connected = false;
}

void TcpClient::socketEvent(short events)
{
	if (events & POLLIN) {
		socketData();
	} else if (events & POLLOUT) { //not both in case client died as part of process_recv
		socketWrite();
	} else if (events & POLLERR || events & POLLHUP) {
		emit disconnected();
	}
}

void TcpClient::run()
{
	struct fdlist pollfds;
	fdlist_create(&pollfds);

	while (!m_shouldExit) {
		{
			QMutexLocker locker(&m_dataLock);
			if (m_sendCache.size() > 0) {
				if (!m_connected)
					m_sendCache.clear();
				else {
					int available_size = m_peer->ob_capacity - m_peer->ob_size;
					int copy_size = m_sendCache.size() < available_size ? m_sendCache.size() : available_size;
					memcpy(m_peer->ob_buf + m_peer->ob_size, m_sendCache.data(), copy_size);
					m_peer->ob_size += copy_size;
					m_peer->events |= POLLOUT;
					m_sendCache.remove(0, copy_size);
				}
			}
		}

		fdlist_reset(&pollfds);
		if (!m_connected)
			fdlist_add(&pollfds, FD_CONNECTED, m_peer->fd, POLLOUT);

		fdlist_add(&pollfds, FD_CLIENT, m_peer->fd, m_peer->events);

		auto cnt = WSAPoll(pollfds.fds, pollfds.count, 10);

		if (cnt == -1) {
			if (errno == EINTR) {
				if (m_shouldExit) {
					break;
				}
			}
		} else {
			for (int i = 0; i < pollfds.count; i++) {
				if (pollfds.fds[i].revents) {
					if (pollfds.owners[i] == FD_CONNECTED) {
						m_connected = true;
						emit connected();
					}

					if (pollfds.owners[i] == FD_CLIENT)
						socketEvent(pollfds.fds[i].revents);
				}
			}
		}
	}

	fdlist_free(&pollfds);
}

TcpServer::TcpServer(QObject *parent) : QThread(parent) {}

TcpServer::~TcpServer() {}

bool TcpServer::startServer(int port)
{
	m_port = port;
	m_listenFD = createSocket();
	if (m_listenFD < 0)
		return false;

	start();
	return true;
}

int TcpServer::createSocket()
{
	WSADATA wsaData = {0};
	auto ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0) {
		qDebug() << "WSAStartup error";
		return -1;
	}

	SOCKET listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenfd == INVALID_SOCKET) {
		qDebug() << "create socket error: " << WSAGetLastError();
		return -1;
	}

	u_long iMode = 1;
	ret = ioctlsocket(listenfd, FIONBIO, &iMode);
	if (ret != NO_ERROR) {
		qDebug() << "set nonblock socket error: " << ret;
		return -1;
	}

	struct sockaddr_in bind_addr;
	memset(&bind_addr, 0, sizeof(bind_addr));

	bind_addr.sin_family = AF_INET;
	bind_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	bind_addr.sin_port = htons(m_port);

	if (bind(listenfd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
		qDebug() << "bind socket error: " << WSAGetLastError();
		return -1;
	}

	if (listen(listenfd, 256) != 0) {
		qDebug() << "listen socket error: " << strerror(errno);
		return -1;
	}

	struct sockaddr_in addr;
	int len = sizeof(addr);
	getsockname(listenfd, (struct sockaddr *)&addr, &len);
	m_port = ntohs(addr.sin_port);

	return listenfd;
}

int TcpServer::clientAccept()
{
	struct sockaddr_in addr;
	int cfd;
	int len = sizeof(struct sockaddr_in);
	cfd = accept(m_listenFD, (struct sockaddr *)&addr, &len);
	if (cfd == INVALID_SOCKET) {
		return -1;
	}

	if (!onNewConnection(cfd)) {
		closesocket(cfd);
		return -2;
	}

	u_long iMode = 1;
	ioctlsocket(cfd, FIONBIO, &iMode);

	int bufsize = 0x20000;
	if (setsockopt(cfd, SOL_SOCKET, SO_SNDBUF, (const char *)&bufsize, sizeof(int)) == -1) {
		qDebug() << "error set socket send buffer";
	}
	if (setsockopt(cfd, SOL_SOCKET, SO_RCVBUF, (const char *)&bufsize, sizeof(int)) == -1) {
		qDebug() << "error set socket recv buffer";
	}

	int yes = 1;
	setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, (const char *)&yes, sizeof(int));

	return cfd;
}

void TcpServer::clientClose(int fd)
{
	closesocket(fd);
	m_clients.remove(fd);

	onConnectionLost(fd);
}

void TcpServer::clientRecv(int fd)
{
	char buf[0x10000] = {0};

	int res = recv(fd, buf, 0x10000, 0);
	if (res <= 0) {
		if (res < 0)
			qDebug("Receive from client fd %d failed: %s", fd, strerror(errno));
		else
			qDebug("Client %d connection closed", fd);
		clientClose(fd);

		return;
	}

	onClientData(fd, buf, res);
}

void TcpServer::clientProcess(int fd, short events)
{
	if (events & POLLIN) {
		clientRecv(fd);
	} else if (events & POLLOUT) { //not both in case client died as part of process_recv

	} else if (events & POLLERR || events & POLLHUP) {
		clientClose(fd);
	}
}

void TcpServer::run()
{
	struct fdlist pollfds;
	fdlist_create(&pollfds);

	while (!m_shouldExit) {

		fdlist_reset(&pollfds);
		fdlist_add(&pollfds, FD_LISTEN, m_listenFD, POLLIN);

		for (auto iter = m_clients.begin(); iter != m_clients.end(); iter++)
			fdlist_add(&pollfds, FD_CLIENT, *iter, POLLIN);

		auto cnt = WSAPoll(pollfds.fds, pollfds.count, 10);

		if (cnt == -1) {
			if (errno == EINTR) {
				if (m_shouldExit) {
					break;
				}
			}
		} else {
			for (int i = 0; i < pollfds.count; i++) {
				if (pollfds.fds[i].revents) {
					if (pollfds.owners[i] == FD_LISTEN) {
						auto fd = clientAccept();
						if (fd < 0) {
							if (fd == -1) {
								fdlist_free(&pollfds);
								goto END;
							}
						} else
							m_clients.insert(fd);
					}
					if (pollfds.owners[i] == FD_CLIENT)
						clientProcess(pollfds.fds[i].fd, pollfds.fds[i].revents);
				}
			}
		}
	}
END:
	fdlist_free(&pollfds);
}

void TcpServer::stopServer()
{
	m_shouldExit = true;
	wait();

	if (m_listenFD > 0) {
		closesocket(m_listenFD);
		m_listenFD = -1;
	}

	for (auto iter = m_clients.begin(); iter != m_clients.end(); iter++) {
		closesocket(*iter);
		onConnectionLost(*iter);
	}
	m_clients.clear();
}
