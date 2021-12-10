//#include "socket.h"
//
//#ifdef WIN32
//#include <winsock2.h>
//#include "winsock2-ext.h"
//#include <windows.h>
//#else
//#include <sys/socket.h>
//#include <sys/un.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <sys/resource.h>
//#include <pwd.h>
//#include <grp.h>
//#endif
//
//#include "log.h"
//#include "utils.h"
//#include "client.h"
//#include <usbmuxd-proto.h>
//#include <stdbool.h>
//
//static int all_interfaces = 0;
// bool should_exit;
//
//#ifdef WIN32
//#define socket_handle SOCKET
//#else
//#define socket_handle int
//#endif
//
//static socket_handle create_socket(void)
//{
//	struct sockaddr_un bind_addr;
//	struct sockaddr_in tcp_addr;
//	socket_handle listenfd;
//
//#ifdef WIN32
//	int ret;
//	WSADATA wsaData = {0};
//
//	// Initialize Winsock
//	usbmuxd_log(LL_INFO, "Starting WSA");
//	ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
//	if (ret != 0) {
//		usbmuxd_log(LL_FATAL, "ERROR: WSAStartup failed: %s",
//			    strerror(ret));
//		return 1;
//	}
//
//	usbmuxd_log(LL_INFO, "Opening socket");
//	listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//	if (listenfd == INVALID_SOCKET) {
//		usbmuxd_log(LL_FATAL, "ERROR: socket() failed: %s",
//			    WSAGetLastError());
//	}
//#else
//	if (!tcp) {
//		if (unlink(socket_path) == -1 && errno != ENOENT) {
//			usbmuxd_log(LL_FATAL, "unlink(%s) failed: %s",
//				    socket_path, strerror(errno));
//			return -1;
//		}
//
//		listenfd = socket(AF_UNIX, SOCK_STREAM, 0);
//		if (listenfd == -1) {
//			usbmuxd_log(LL_FATAL, "ERROR: socket() failed: %s",
//				    strerror(errno));
//			return -1;
//		}
//	} else {
//		listenfd = socket(AF_INET, SOCK_STREAM, 0);
//		if (listenfd == -1) {
//			usbmuxd_log(LL_FATAL, "ERROR: socket() failed: %s",
//				    strerror(errno));
//			return -1;
//		}
//	}
//#endif
//
//#ifdef WIN32
//	u_long iMode = 1;
//
//	usbmuxd_log(LL_INFO, "Setting socket to non-blocking");
//	ret = ioctlsocket(listenfd, FIONBIO, &iMode);
//	if (ret != NO_ERROR) {
//		usbmuxd_log(LL_FATAL,
//			    "ERROR: Could not set socket to non blocking: %d",
//			    ret);
//		return -1;
//	}
//#else
//	int flags = fcntl(listenfd, F_GETFL, 0);
//	if (flags < 0) {
//		usbmuxd_log(LL_FATAL, "ERROR: Could not get flags for socket");
//	} else {
//		if (fcntl(listenfd, F_SETFL, flags | O_NONBLOCK) < 0) {
//			usbmuxd_log(
//				LL_FATAL,
//				"ERROR: Could not set socket to non-blocking");
//		}
//	}
//#endif
//
//	memset(&bind_addr, 0, sizeof(bind_addr));
//
//#ifdef WIN32
//	bind_addr.sin_family = AF_INET;
//	bind_addr.sin_addr.s_addr = all_interfaces == 0 ? inet_addr("127.0.0.1")
//							: inet_addr("0.0.0.0");
//	bind_addr.sin_port = htons(USBMUXD_SOCKET_PORT);
//#else
//	if (tcp) {
//		usbmuxd_log(LL_INFO, "Preparing a TCP socket");
//		tcp_addr.sin_family = AF_INET;
//		tcp_addr.sin_addr.s_addr = all_interfaces == 0
//						   ? inet_addr("127.0.0.1")
//						   : inet_addr("0.0.0.0");
//		tcp_addr.sin_port = htons(USBMUXD_SOCKET_PORT);
//	} else {
//		usbmuxd_log(LL_INFO, "Preparing a Unix socket");
//		bind_addr.sun_family = AF_UNIX;
//		strcpy(bind_addr.sun_path, socket_path);
//	}
//#endif
//
//	usbmuxd_log(LL_INFO, "Binding to socket");
//#ifdef WIN32
//	if (bind(listenfd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) !=
//	    0) {
//		usbmuxd_log(
//			LL_FATAL,
//			"bind() failed. WSAGetLastError returned error code %u. Is another process using TCP port %d?",
//			WSAGetLastError(), USBMUXD_SOCKET_PORT);
//		return -1;
//	}
//#else
//	if (tcp) {
//		if (bind(listenfd, (struct sockaddr *)&tcp_addr,
//			 sizeof(tcp_addr)) != 0) {
//			usbmuxd_log(LL_FATAL,
//				    "bind() on a Unix socket failed: %s",
//				    strerror(errno));
//			return -1;
//		}
//	} else {
//		if (bind(listenfd, (struct sockaddr *)&bind_addr,
//			 sizeof(bind_addr)) != 0) {
//			usbmuxd_log(LL_FATAL,
//				    "bind() on a TCP socket failed: %s",
//				    strerror(errno));
//			return -1;
//		}
//	}
//#endif
//
//	// Start listening
//	usbmuxd_log(LL_INFO, "Starting to listen on socket");
//	if (listen(listenfd, 5) != 0) {
//		usbmuxd_log(LL_FATAL, "listen() failed: %s", strerror(errno));
//		return -1;
//	}
//
//#ifndef WIN32
//	chmod(socket_path, 0666);
//#endif
//
//	return listenfd;
//}
//
//#if (!HAVE_PPOLL && !WIN32)
//static int ppoll(struct pollfd *fds, nfds_t nfds,
//		 const struct timespec *timeout, const sigset_t *sigmask)
//{
//	int ready;
//	sigset_t origmask;
//	int to = timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000;
//
//	sigprocmask(SIG_SETMASK, sigmask, &origmask);
//	ready = poll(fds, nfds, to);
//	sigprocmask(SIG_SETMASK, &origmask, NULL);
//
//	return ready;
//}
//#endif
//
//#ifdef WIN32
//static int ppoll(struct WSAPoll *fds, nfds_t nfds, int timeout)
//{
//	return WSAPoll(fds, nfds, timeout);
//}
//#endif
//
//void *socket_thread(void *data)
//{
//	socket_handle listenfd = create_socket();
//	if (listenfd < 0)
//		return NULL;
//
//	int to, cnt, i, dto;
//	struct fdlist pollfds;
//
//#ifndef WIN32
//	struct timespec tspec;
//
//	sigset_t empty_sigset;
//	sigemptyset(&empty_sigset); // unmask all signals
//#endif
//
//	fdlist_create(&pollfds);
//	while (!should_exit) {
//#ifndef WIN32
//		to = usb_get_timeout();
//		usbmuxd_log(LL_FLOOD, "USB timeout is %d ms", to);
//		dto = device_get_timeout();
//		usbmuxd_log(LL_FLOOD, "Device timeout is %d ms", dto);
//		if (dto < to)
//			to = dto;
//#endif
//
//#ifdef WIN32
//		Sleep(1);
//#endif
//
//		fdlist_reset(&pollfds);
//		fdlist_add(&pollfds, FD_LISTEN, listenfd, POLLIN);
//
//#ifndef WIN32
//		// Polling of USB events is not available through libusb on Windows,
//		// see http://libusb.org/static/api-1.0/group__poll.html
//		usb_get_fds(&pollfds);
//#endif
//
//		client_get_fds(&pollfds);
//
//#ifdef WIN32
//		cnt = ppoll(pollfds.fds, pollfds.count, 100);
//#else
//		tspec.tv_sec = to / 1000;
//		tspec.tv_nsec = (to % 1000) * 1000000;
//		cnt = ppoll(pollfds.fds, pollfds.count, &tspec, &empty_sigset);
//#endif
//
//		if (cnt == -1) {
//			if (errno == EINTR) {
//				if (should_exit) {
//					usbmuxd_log(
//						LL_INFO,
//						"Event processing interrupted");
//					break;
//				}
//			}
//#ifndef WIN32
//		} else if (cnt == 0) {
//			if (usb_process() < 0) {
//				usbmuxd_log(LL_FATAL, "usb_process() failed");
//				fdlist_free(&pollfds);
//				return -1;
//			}
//			device_check_timeouts();
//#endif
//		} else {
//			for (i = 0; i < pollfds.count; i++) {
//				if (pollfds.fds[i].revents) {
//					if (pollfds.owners[i] == FD_LISTEN) {
//						if (client_accept(listenfd) <
//						    0) {
//							usbmuxd_log(
//								LL_FATAL,
//								"client_accept() failed");
//							fdlist_free(&pollfds);
//							return -1;
//						}
//					}
//					if (pollfds.owners[i] == FD_CLIENT) {
//						client_process(
//							pollfds.fds[i].fd,
//							pollfds.fds[i].revents);
//					}
//				}
//			}
//		}
//	}
//	fdlist_free(&pollfds);
//
//	return NULL;
//}
//
//pthread_t create_socket_thread()
//{
//	pthread_t th;
//	pthread_create(&th, NULL, socket_thread, NULL);
//	return th;
//}
