#include "MirrorManager.h"
#include "tcp.h"
#include "endianness.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QTimer>
#include <QThread>
#include <QApplication>

#define LOCKDOWN_PROTOCOL_VERSION "2"

/**
 * Internally used function for cleaning up SSL stuff.
 */
static void internal_ssl_cleanup(ssl_data_t ssl_data)
{
	if (!ssl_data)
		return;

	if (ssl_data->session) {
		SSL_free(ssl_data->session);
	}
	if (ssl_data->ctx) {
		SSL_CTX_free(ssl_data->ctx);
	}
}

static int ssl_verify_callback(int ok, X509_STORE_CTX *ctx)
{
	(void)ctx;
	(void)ok;
	return 1;
}

#ifndef STRIP_DEBUG_CODE
static const char *ssl_error_to_string(int e)
{
	switch (e) {
	case SSL_ERROR_NONE:
		return "SSL_ERROR_NONE";
	case SSL_ERROR_SSL:
		return "SSL_ERROR_SSL";
	case SSL_ERROR_WANT_READ:
		return "SSL_ERROR_WANT_READ";
	case SSL_ERROR_WANT_WRITE:
		return "SSL_ERROR_WANT_WRITE";
	case SSL_ERROR_WANT_X509_LOOKUP:
		return "SSL_ERROR_WANT_X509_LOOKUP";
	case SSL_ERROR_SYSCALL:
		return "SSL_ERROR_SYSCALL";
	case SSL_ERROR_ZERO_RETURN:
		return "SSL_ERROR_ZERO_RETURN";
	case SSL_ERROR_WANT_CONNECT:
		return "SSL_ERROR_WANT_CONNECT";
	case SSL_ERROR_WANT_ACCEPT:
		return "SSL_ERROR_WANT_ACCEPT";
	default:
		return "UNKOWN_ERROR_VALUE";
	}
}
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L || \
	(defined(LIBRESSL_VERSION_NUMBER) && (LIBRESSL_VERSION_NUMBER < 0x20020000L))
#define TLS_method TLSv1_method
#endif

#if OPENSSL_VERSION_NUMBER < 0x10002000L || defined(LIBRESSL_VERSION_NUMBER)
static void SSL_COMP_free_compression_methods(void)
{
	sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
}
#endif

static void openssl_remove_thread_state(void)
{
/*  ERR_remove_thread_state() is available since OpenSSL 1.0.0-beta1, but
 *  deprecated in OpenSSL 1.1.0 */
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
#if OPENSSL_VERSION_NUMBER >= 0x10000001L
	ERR_remove_thread_state(NULL);
#else
	ERR_remove_state(0);
#endif
#endif
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
static mutex_t *mutex_buf = NULL;
static void locking_function(int mode, int n, const char* file, int line)
{
	if (mode & CRYPTO_LOCK)
		mutex_lock(&mutex_buf[n]);
	else
		mutex_unlock(&mutex_buf[n]);
}

#if OPENSSL_VERSION_NUMBER < 0x10000000L
static unsigned long id_function(void)
{
	return ((unsigned long)THREAD_ID);
}
#else
static void id_function(CRYPTO_THREADID *thread)
{
	CRYPTO_THREADID_set_numeric(thread, (unsigned long)THREAD_ID);
}
#endif
#endif

#include <winsock.h>
int socket_connect(uint16_t port)
{
	int sfd = -1;
	int yes = 1;
	int bufsize = 0x20000;

	u_long l_no = 0;
	WSADATA wsa_data;
	static int wsa_init = 0;
	if (!wsa_init) {
		if (WSAStartup(MAKEWORD(2,2), &wsa_data) != ERROR_SUCCESS) {
			qFatal("WSAStartup failed!");
			ExitProcess(0);
		}
		wsa_init = 1;
	}

	sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sfd == -1) {
		return -1;
	}

	ioctlsocket(sfd, FIONBIO, &l_no);

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr)); 
	serv_addr.sin_family = AF_INET;          
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
	serv_addr.sin_port = htons(port); 
	if (connect(sfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
		return -1;

	if (setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (const char *)&yes, sizeof(int)) == -1) {
		perror("Could not set TCP_NODELAY on socket");
	}

	if (setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, (const char *)&bufsize, sizeof(int)) == -1) {
		perror("Could not set send buffer for socket");
	}

	if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, (const char *)&bufsize, sizeof(int)) == -1) {
		perror("Could not set receive buffer for socket");
	}
	
	return sfd;
}

MirrorManager::MirrorManager()
{
	ipc_client_create(&ipc_client);
	connect(this, &QObject::destroyed, qApp, &QApplication::quit);

	auto findConn = [this](){
		ConnectionInfo *conn = NULL;
		for (auto iter = m_connections.begin(); iter != m_connections.end(); iter++)
		{
			if((*iter)->type == InitPair && (*iter)->connState == MuxConnState::CONN_CONNECTED)
				conn = *iter;
		}
		return conn;
	};

	usb_init();
	m_pktbuf = (unsigned char *)malloc(DEV_MRU);
	m_pairBlockEvent = new QEventLoop(this);
	m_usbDataBlockEvent = new QEventLoop(this);
	m_notificationTimer = new QTimer(this);

	serverSocket = new QTcpServer(this);
	m_handshakeBlockEvent = new QEventLoop(this);
	if (serverSocket->listen()) {
		connect(serverSocket, &QTcpServer::newConnection, this, [=](){
			qDebug() << "new connection accepted.";
			auto client = serverSocket->nextPendingConnection();
			auto c = findConn();
			if (c)
				c->clientSocketInServerSide = client;

			connect(client, &QTcpSocket::readyRead, [=](){
				auto all = client->readAll();
				qDebug() << "socket recv size: " << all.size();
				ConnectionInfo *conn = findConn();
				if (!conn)
					return;

				sendTcp(conn, TH_ACK, (const unsigned char *)all.data(), all.size());
			});
		});
	}
}

MirrorManager::~MirrorManager()
{
	qDebug() << "MirrorManager destroyed.";
	free(m_pktbuf);
	ipc_client_destroy(&ipc_client);
}

void MirrorManager::quit()
{
	stopScreenMirror();

	if (m_inPair)
		pairError(u8"final stop");
	else
		clearDeviceResource(true);

	deleteLater();
}

void MirrorManager::readUSBData(void *data)
{
	MirrorManager *manager = (MirrorManager *)data;
	char *buffer = (char *)malloc(DEV_MRU);
	int readLen = 0;
	while (!manager->m_stop) {
		readLen = usb_bulk_read(manager->m_deviceHandle, manager->ep_in, buffer, DEV_MRU, 200);
		if (readLen < 0) {
			if (readLen != -116) {
				qDebug() << "usb_bulk_read fail: " << readLen;
				QMetaObject::invokeMethod(manager, "pairError", Q_ARG(QString, u8"停止USB读取线程"), Q_ARG(bool ,false));
				break;
			}
		} else
			QMetaObject::invokeMethod(manager, "onDeviceData", Q_ARG(QByteArray, QByteArray(buffer, readLen)));
	}

	free(buffer);

	qDebug() << "pair read usb data thread stopped...";
}

void MirrorManager::clearPairResource()
{
	m_stop = true;
	if (m_usbReadTh.joinable())
		m_usbReadTh.join();

	if (m_notificationTimer->isActive())
		m_notificationTimer->stop();

	for (auto iter = m_connections.begin(); iter != m_connections.end(); iter++) {
		auto c = *iter;
		clearHandshakeResource(c);
		delete c;
	}
	m_connections.clear();

	qDebug() << "connections cleared...";
}

void MirrorManager::clearDeviceResource(bool doClose)
{
	qDebug() << "clearDeviceResource, do close: " << doClose;
	if (doClose && m_deviceHandle) {
		usb_release_interface(m_deviceHandle, m_interface);
		usb_release_interface(m_deviceHandle, m_interface_fa);
		usb_close(m_deviceHandle);
	}
	m_deviceHandle = NULL;
	m_device = NULL;
}

void MirrorManager::pairError(QString msg, bool closeDevice)
{
	qDebug() << "enter pairError";
	m_errorMsg = msg;
	
	clearPairResource();
	clearDeviceResource(closeDevice);

	m_usbDataBlockEvent->quit();
	m_pairBlockEvent->quit();
	qDebug() << "pairError called+++++++++++++++++++++";
}

void MirrorManager::startConnect(ConnectionType type, uint16_t dport)
{
	ConnectionInfo *info = new ConnectionInfo(type, m_initPort++, dport);
	m_connections.append(info);

	sendTcp(info, TH_SYN, NULL, 0);
}

void MirrorManager::onDeviceVersionInput(VersionHeader *vh)
{
	if(m_devState != MuxDevState::MUXDEV_INIT) {
		return;
	}
	vh->major = ntohl(vh->major);
	vh->minor = ntohl(vh->minor);
	if(vh->major != 2 && vh->major != 1) {
		qDebug("Device has unknown version %d.%d", vh->major, vh->minor);
		return;
	}
	m_devVersion = vh->major;

	if (m_devVersion >= 2) {
		sendPacket(MuxProtocol::MUX_PROTO_SETUP, NULL, "\x07", 1);
	}

	qDebug("Connected to v%d.%d device on location 0x%x with serial number %s", m_devVersion, vh->minor, m_device->bus->location, m_serial);
	m_devState = MuxDevState::MUXDEV_ACTIVE;
	
	startConnect(InitPair, 0xf27e);
}

static bool lockdownCheckResult(plist_t dict, const char *query_match)
{
	bool ret = false;
	plist_t query_node = plist_dict_get_item(dict, "Request");
	if (!query_node) {
		return ret;
	}

	if (plist_get_node_type(query_node) != PLIST_STRING) {
		return ret;
	} else {
		char *query_value = NULL;

		plist_get_string_val(query_node, &query_value);
		if (!query_value) {
			return ret;
		}

		if (query_match && (strcmp(query_value, query_match) != 0)) {
			free(query_value);
			return ret;
		}

		free(query_value);
	}

	plist_t result_node = plist_dict_get_item(dict, "Result");
	if (!result_node) {
		/* iOS 5: the 'Result' key is not present anymore.
		   But we need to check for the 'Error' key. */
		plist_t err_node = plist_dict_get_item(dict, "Error");
		if (err_node) {
			if (plist_get_node_type(err_node) == PLIST_STRING) {
				char *err_value = NULL;

				plist_get_string_val(err_node, &err_value);
				if (err_value) {
					qDebug("ERROR: %s", err_value);
					free(err_value);
				} else {
					qDebug("ERROR: unknown error occurred");
				}
			}
			return ret;
		}

		ret = true;

		return ret;
	}

	plist_type result_type = plist_get_node_type(result_node);
	if (result_type == PLIST_STRING) {
		char *result_value = NULL;

		plist_get_string_val(result_node, &result_value);
		if (result_value) {
			if (!strcmp(result_value, "Success")) {
				ret = true;
			} else if (!strcmp(result_value, "Failure")) {
				
			} else {
				qDebug("ERROR: unknown result value '%s'", result_value);
			}
		}

		if (result_value)
			free(result_value);
	}

	return ret;
}

static void plist_dict_add_label(plist_t plist, const char *label)
{
	if (plist && label) {
		if (plist_get_node_type(plist) == PLIST_DICT)
			plist_dict_set_item(plist, "Label", plist_new_string(label));
	}
}

bool MirrorManager::receivePlist(ConnectionInfo *conn, plist_t *ret, const char *key, bool allowTimeout)
{
	uint32_t pktlen = 0;
	char *buf = NULL;
	if (conn->ssl_data) {
		if (!readDataFromSSL(conn, &pktlen, sizeof(pktlen)))
			return false;

		pktlen = be32toh(pktlen);
		buf = (char *)malloc(pktlen);
		if (!readDataFromSSL(conn, (void*)buf, pktlen)) {
			free(buf);
			return false;
		}
	} else {
		if (!readDataWithSize(conn, &pktlen, sizeof(pktlen), allowTimeout)) {
			qDebug() << QString("read %1 length failed").arg(key);
			return false;
		}

		pktlen = be32toh(pktlen);
		buf = (char *)malloc(pktlen);
		if (!readDataWithSize(conn, buf, pktlen, allowTimeout)) {
			qDebug() << QString("read %1 data failed").arg(key);
			free(buf);
			return false;
		}
	}

	*ret = NULL;
	bool success = receivePlistInternal(buf, pktlen, ret);
	free(buf);
	if (!success) {
		pairError(QString(u8"%1 响应数据解析异常。").arg(key));
		return false;
	}

	return true;
}

bool MirrorManager::lockdownGetValue(ConnectionInfo *conn, const char *domain, const char *key, plist_t *value)
{
	bool ret = false;
	plist_t dict = plist_new_dict();
	plist_dict_add_label(dict, "usbmuxd");
	if (domain) {
		plist_dict_set_item(dict, "Domain", plist_new_string(domain));
	}
	if (key) {
		plist_dict_set_item(dict, "Key", plist_new_string(key));
	}
	plist_dict_set_item(dict, "Request", plist_new_string("GetValue"));
	sendPlist(conn, dict, false);
	plist_free(dict);

	if (!receivePlist(conn, &dict, key)) {
		return ret;
	}

	if (lockdownCheckResult(dict, "GetValue")) {
		plist_t value_node = plist_dict_get_item(dict, "Value");
		if (value_node) {
			qDebug("has a value");
			*value = plist_copy(value_node);
			ret = true;
		} else
			pairError(QString(u8"GetValue 返回中没有 %1").arg(key));

	} else {
		pairError(u8"GetValue 返回校验失败。");
	}

	plist_free(dict);
	return ret;
}

bool MirrorManager::lockdownSetValue(ConnectionInfo *conn, const char *domain, const char *key, plist_t value)
{
	bool ret = false;
	plist_t dict = plist_new_dict();
	plist_dict_add_label(dict, "usbmuxd");
	if (domain) {
		plist_dict_set_item(dict,"Domain", plist_new_string(domain));
	}
	if (key) {
		plist_dict_set_item(dict,"Key", plist_new_string(key));
	}
	plist_dict_set_item(dict,"Request", plist_new_string("SetValue"));
	plist_dict_set_item(dict,"Value", value);
	sendPlist(conn, dict, false);
	plist_free(dict);

	if (!receivePlist(conn, &dict, key)) {
		return ret;
	}

	if (lockdownCheckResult(dict, "SetValue"))
		ret = true;
	else 
		pairError(u8"GetValue 返回校验失败。");

	plist_free(dict);
	return ret;
}

static plist_t lockdowndPairRecordToPlist(MirrorManager::PairRecord *pair_record)
{
	if (!pair_record)
		return NULL;

	/* setup request plist */
	plist_t dict = plist_new_dict();
	plist_dict_set_item(dict, "DeviceCertificate", plist_new_data(pair_record->device_certificate, strlen(pair_record->device_certificate)));
	plist_dict_set_item(dict, "HostCertificate", plist_new_data(pair_record->host_certificate, strlen(pair_record->host_certificate)));
	plist_dict_set_item(dict, "HostID", plist_new_string(pair_record->host_id));
	plist_dict_set_item(dict, "RootCertificate", plist_new_data(pair_record->root_certificate, strlen(pair_record->root_certificate)));
	plist_dict_set_item(dict, "SystemBUID", plist_new_string(pair_record->system_buid));

	return dict;
}

bool MirrorManager::lockdownPair(ConnectionInfo *conn, PairRecord *record, bool raiseError)
{
	plist_t options = plist_new_dict();
	plist_dict_set_item(options, "ExtendedPairingErrors", plist_new_bool(1));

	bool ret = lockdownDoPair(conn, record, "Pair", options, NULL, raiseError);

	plist_free(options);

	return ret;
}

bool MirrorManager::lockdownGetDevicePublicKeyAsKeyData(ConnectionInfo *conn, key_data_t *public_key)
{
	bool ret = false;
	plist_t value = NULL;
	char *value_value = NULL;
	uint64_t size = 0;

	ret = lockdownGetValue(conn, NULL, "DevicePublicKey", &value);
	if (!ret) {
		return ret;
	}
	plist_get_data_val(value, &value_value, &size);
	public_key->data = (unsigned char *)value_value;
	public_key->size = size;

	plist_free(value);
	value = NULL;

	return ret;
}

bool MirrorManager::lockdownPairRecordgenerate(ConnectionInfo *conn, plist_t *pair_record)
{
	bool ret = false;

	key_data_t public_key = { NULL, 0 };
	char* host_id = NULL;
	char* system_buid = NULL;

	/* retrieve device public key */
	ret = lockdownGetDevicePublicKeyAsKeyData(conn, &public_key);
	if (!ret) {
		qDebug("device refused to send public key.");
		goto leave;
	}
	qDebug("device public key follows:\n%.*s", public_key.size, public_key.data);

	*pair_record = plist_new_dict();

	/* generate keys and certificates into pair record */
	userpref_error_t uret = USERPREF_E_SUCCESS;
	uret = pair_record_generate_keys_and_certs(*pair_record, public_key);
	ret = uret == USERPREF_E_SUCCESS;

	/* set SystemBUID */
	userpref_read_system_buid(&system_buid);
	if (system_buid) {
		plist_dict_set_item(*pair_record, USERPREF_SYSTEM_BUID_KEY, plist_new_string(system_buid));
	}

	/* set HostID */
	host_id = generate_uuid();
	pair_record_set_host_id(*pair_record, host_id);

leave:
	if (host_id)
		free(host_id);
	if (system_buid)
		free(system_buid);
	if (public_key.data)
		free(public_key.data);

	return ret;
}

bool MirrorManager::lockdownDoPair(ConnectionInfo *conn, PairRecord *pair_record, const char *verb, plist_t options, plist_t *result, bool raiseError)
{
	plist_t dict = NULL;
	plist_t pair_record_plist = NULL;
	plist_t wifi_node = NULL;
	int pairing_mode = 0; /* 0 = libimobiledevice, 1 = external */

	if (pair_record && pair_record->system_buid && pair_record->host_id) {
		/* valid pair_record passed? */
		if (!pair_record->device_certificate || !pair_record->host_certificate || !pair_record->root_certificate) {
			pairError(u8"传递的配对信息缺少必要信息。");
			return false;
		}

		/* use passed pair_record */
		pair_record_plist = lockdowndPairRecordToPlist(pair_record);

		pairing_mode = 1;
	} else {
		/* generate a new pair record if pairing */
		if (!strcmp("Pair", verb)) {
			bool ret = lockdownPairRecordgenerate(conn, &pair_record_plist);

			if (!ret) {
				if (pair_record_plist)
					plist_free(pair_record_plist);

				pairError(u8"生成配对信息有误。");
				return ret;
			}

			/* get wifi mac now, if we get it later we fail on iOS 7 which causes a reconnect */
			lockdownGetValue(conn, NULL, "WiFiAddress", &wifi_node);
		} else {
			/* use existing pair record */
			userpref_read_pair_record(m_serial, &pair_record_plist);
			if (!pair_record_plist) {
				pairError(u8"读取保存的host id有误。");
				return false;
			}
		}
	}

	plist_t request_pair_record = plist_copy(pair_record_plist);

	/* remove stuff that is private */
	plist_dict_remove_item(request_pair_record, USERPREF_ROOT_PRIVATE_KEY_KEY);
	plist_dict_remove_item(request_pair_record, USERPREF_HOST_PRIVATE_KEY_KEY);

	/* setup pair request plist */
	dict = plist_new_dict();
	plist_dict_add_label(dict, "usbmuxd");
	plist_dict_set_item(dict, "PairRecord", request_pair_record);
	plist_dict_set_item(dict, "Request", plist_new_string(verb));
	plist_dict_set_item(dict, "ProtocolVersion", plist_new_string(LOCKDOWN_PROTOCOL_VERSION));

	if (options) {
		plist_dict_set_item(dict, "PairingOptions", plist_copy(options));
	}

	/* send to device */
	sendPlist(conn, dict, false);
	plist_free(dict);
	dict = NULL;

	/* Now get device's answer */
	bool ret = receivePlist(conn, &dict, "LockDownPair");

	if (!ret) {
		plist_free(pair_record_plist);
		if (wifi_node)
			plist_free(wifi_node);
		return ret;
	}

	if (strcmp(verb, "Unpair") == 0) {
		/* workaround for Unpair giving back ValidatePair,
		 * seems to be a bug in the device's fw */
		if (!lockdownCheckResult(dict, NULL)) {
			ret = false;
		}
	} else {
		if (!lockdownCheckResult(dict, verb)) {
			ret = false;
		}
	}

	/* if pairing succeeded */
	if (ret) {
		qDebug("%s success", verb);
		if (!pairing_mode) {
			qDebug("internal pairing mode");
			if (!strcmp("Unpair", verb)) {
				/* remove public key from config */
				userpref_delete_pair_record(m_serial);
			} else {
				if (!strcmp("Pair", verb)) {
					/* add returned escrow bag if available */
					plist_t extra_node = plist_dict_get_item(dict, USERPREF_ESCROW_BAG_KEY);
					if (extra_node && plist_get_node_type(extra_node) == PLIST_DATA) {
						qDebug("Saving EscrowBag from response in pair record");
						plist_dict_set_item(pair_record_plist, USERPREF_ESCROW_BAG_KEY, plist_copy(extra_node));
					}

					/* save previously retrieved wifi mac address in pair record */
					if (wifi_node) {
						qDebug("Saving WiFiAddress from device in pair record");
						plist_dict_set_item(pair_record_plist, USERPREF_WIFI_MAC_ADDRESS_KEY, plist_copy(wifi_node));
						plist_free(wifi_node);
						wifi_node = NULL;
					}

					userpref_save_pair_record(m_serial, pair_record_plist);
				}
			}
		} else {
			qDebug("external pairing mode");
		}
	} else {
		qDebug("%s failure", verb);
		plist_t error_node = NULL;
		/* verify error condition */
		error_node = plist_dict_get_item(dict, "Error");
		if (error_node) {
			char *value = NULL;
			plist_get_string_val(error_node, &value);
			if (value) {
				/* the first pairing fails if the device is password protected */
				ret = false;
				if (strcmp(value, "UserDeniedPairing") == 0)
					pairError(QString("用户拒绝了配对请求，请插拔后再试。"), false); //why? 这里close了device之后，有几率下次插入时set configuration失败
				else {
					if (raiseError)
						pairError(QString(u8"配对失败, %1").arg(value));
				}
				free(value);
			}
		}
	}

	if (pair_record_plist) {
		plist_free(pair_record_plist);
		pair_record_plist = NULL;
	}

	if (wifi_node) {
		plist_free(wifi_node);
		wifi_node = NULL;
	}

	if (result) {
		*result = dict;
	} else {
		plist_free(dict);
		dict = NULL;
	}

	return ret;
}

void MirrorManager::setUntrustedHostBuid(ConnectionInfo *conn)
{
	char* system_buid = NULL;
	config_get_system_buid(&system_buid);
	qDebug("%s: Setting UntrustedHostBUID to %s", __func__, system_buid);
	lockdownSetValue(conn, NULL, "UntrustedHostBUID", plist_new_string(system_buid));
	free(system_buid);
}

bool MirrorManager::lockdownBuildStartServiceRequest(const char *identifier, int send_escrow_bag, plist_t *request)
{
	plist_t dict = plist_new_dict();

	/* create the basic request params */
	plist_dict_add_label(dict, "usbmuxd");
	plist_dict_set_item(dict, "Request", plist_new_string("StartService"));
	plist_dict_set_item(dict, "Service", plist_new_string(identifier));

	/* if needed - get the escrow bag for the device and send it with the request */
	if (send_escrow_bag) {
		/* get the pairing record */
		plist_t pair_record = NULL;
		userpref_read_pair_record(m_serial, &pair_record);
		if (!pair_record) {
			qDebug("ERROR: failed to read pair record for device: %s", m_serial);
			plist_free(dict);
			pairError(u8"从配对记录中读取信息失败。");
			return false;
		}

		/* try to read the escrow bag from the record */
		plist_t escrow_bag = plist_dict_get_item(pair_record, USERPREF_ESCROW_BAG_KEY);
		if (!escrow_bag || (PLIST_DATA != plist_get_node_type(escrow_bag))) {
			qDebug("ERROR: Failed to retrieve the escrow bag from the device's record");
			plist_free(dict);
			plist_free(pair_record);
			pairError(u8"ERROR: Failed to retrieve the escrow bag from the device's record 从配对记录中读取信息失败。");
			return false;
		}

		qDebug("Adding escrow bag to StartService for %s", identifier);
		plist_dict_set_item(dict, USERPREF_ESCROW_BAG_KEY, plist_copy(escrow_bag));
		plist_free(pair_record);
	}

	*request = dict;
	return true;
}

bool MirrorManager::lockdownStartService(ConnectionInfo *conn, const char *identifier, LockdownServiceDescriptor **service)
{
	return lockdownDoStartService(conn, identifier, 0, service);
}

bool MirrorManager::lockdownDoStartService(ConnectionInfo *conn, const char *identifier, int send_escrow_bag, LockdownServiceDescriptor **service)
{
	if (!identifier || !service) {
		pairError(u8"lockdownDoStartService 参数有误。");
		return false;
	}

	if (*service) {
		// reset fields if service descriptor is reused
		(*service)->port = 0;
		(*service)->ssl_enabled = 0;
	}

	plist_t dict = NULL;
	uint16_t port_loc = 0;

	/* create StartService request */
	bool ret = lockdownBuildStartServiceRequest(identifier, send_escrow_bag, &dict);
	if (!ret)
		return ret;

	/* send to device */
	sendPlist(conn, dict, false);
	plist_free(dict);
	dict = NULL;

	ret = receivePlist(conn, &dict, "lockdownDoStartService");

	if (!ret)
		return ret;

	ret = lockdownCheckResult(dict, "StartService");
	if (ret) {
		if (*service == NULL)
			*service = (LockdownServiceDescriptor *)malloc(sizeof(struct LockdownServiceDescriptor));
		(*service)->port = 0;
		(*service)->ssl_enabled = 0;

		/* read service port number */
		plist_t node = plist_dict_get_item(dict, "Port");
		if (node && (plist_get_node_type(node) == PLIST_UINT)) {
			uint64_t port_value = 0;
			plist_get_uint_val(node, &port_value);

			if (port_value)
				port_loc = port_value;
			
			if (port_loc) 
				(*service)->port = port_loc;
		}

		/* check if the service requires SSL */
		node = plist_dict_get_item(dict, "EnableServiceSSL");
		if (node && (plist_get_node_type(node) == PLIST_BOOLEAN)) {
			uint8_t b = 0;
			plist_get_bool_val(node, &b);
			(*service)->ssl_enabled = b;
		}
	} else {
		plist_t error_node = plist_dict_get_item(dict, "Error");
		if (error_node && PLIST_STRING == plist_get_node_type(error_node)) {
			char *error = NULL;
			plist_get_string_val(error_node, &error);
			free(error);
		}
		pairError(u8"StartService返回检查失败。");
	}

	plist_free(dict);
	dict = NULL;

	return ret;
}

void MirrorManager::handleVersionValue(ConnectionInfo *conn, plist_t value)
{
	if (value && plist_get_node_type(value) == PLIST_STRING) {
		char *versionStr = NULL;
		plist_get_string_val(value, &versionStr);
		int versionMajor = strtol(versionStr, NULL, 10);
		if (versionMajor < 7)
			pairError(u8"iOS版本太低，投屏所需最低版本为iOS7。");
		else {
			qDebug("%s: Found ProductVersion %s device %s", __func__, versionStr, m_serial);
			setUntrustedHostBuid(conn);

			if (lockdownPair(conn, NULL)) {
				pairSuccess(conn);
			} else {
				LockdownServiceDescriptor *service = NULL;
				bool ret = lockdownStartService(conn, "com.apple.mobile.insecure_notification_proxy", &service);
				if (ret) {
					startConnect(StartService, service->port);
					free(service);
					service = NULL;
				}
			}
		}

		free(versionStr);
	} else
		pairError(u8"ProductionVersion 获取version字符串失败。");

	plist_free(value);
}

bool MirrorManager::lockdownStopSession(ConnectionInfo *conn, const char *session_id)
{
	bool ret = false;

	plist_t dict = plist_new_dict();
	plist_dict_add_label(dict, "usbmuxd");
	plist_dict_set_item(dict,"Request", plist_new_string("StopSession"));
	plist_dict_set_item(dict,"SessionID", plist_new_string(session_id));

	qDebug("stopping session %s", session_id);

	ret = sendPlist(conn, dict, false);

	plist_free(dict);
	dict = NULL;

	ret = receivePlist(conn, &dict, "StopSession");

	if (!ret || !dict) {
		qDebug("LOCKDOWN_E_PLIST_ERROR");
		return false;
	}

	ret = lockdownCheckResult(dict, "StopSession");
	if (ret) {
		qDebug("success");
	}

	plist_free(dict);
	dict = NULL;

	if (conn->sessionId) {
		free(conn->sessionId);
		conn->sessionId = NULL;
	}

	if (conn->sslEnabled) {
		lockdownDisableSSL(conn);
		conn->sslEnabled = false;
	}

	return ret;
}

void MirrorManager::clearHandshakeResource(ConnectionInfo *conn)
{
	if (!m_connections.contains(conn))
		return;

	QMutexLocker locker(&conn->deleteMutex);

	if (conn->sessionId) {
		free(conn->sessionId);
		conn->sessionId = NULL;
	}

	if (conn->ssl_data) {
		internal_ssl_cleanup(conn->ssl_data);
		free(conn->ssl_data);
		conn->ssl_data = NULL;
	}

	if (conn->clientSocktFD != -1) {
		closesocket(conn->clientSocktFD);
		conn->clientSocktFD = -1;
	}

	if (conn->clientSocketInServerSide) {
		conn->clientSocketInServerSide->deleteLater();
		conn->clientSocketInServerSide = nullptr;
	}
}

void MirrorManager::removeConnection(ConnectionInfo *conn)
{
	clearHandshakeResource(conn);
	m_connections.removeOne(conn);
	delete conn;
}

bool MirrorManager::lockdownEnableSSL(ConnectionInfo *conn)
{
	bool ret = false;
	uint32_t return_me = 0;
	plist_t pair_record = NULL;

	userpref_read_pair_record(m_serial, &pair_record);
	if (!pair_record) {
		qDebug("ERROR: Failed enabling SSL. Unable to read pair record for udid %s.", m_serial);
		return ret;
	}

	key_data_t root_cert = { NULL, 0 };
	key_data_t root_privkey = { NULL, 0 };

	pair_record_import_crt_with_name(pair_record, USERPREF_ROOT_CERTIFICATE_KEY, &root_cert);
	pair_record_import_key_with_name(pair_record, USERPREF_ROOT_PRIVATE_KEY_KEY, &root_privkey);

	if (pair_record)
		plist_free(pair_record);

	conn->clientSocktFD = socket_connect(serverSocket->serverPort());
	if (conn->clientSocktFD < 0) {
		qDebug("connect local socket error");
		pairError(u8"ssl握手连接本地socket出错。");
		return ret;
	}

	BIO *ssl_bio = BIO_new(BIO_s_socket());
	if (!ssl_bio) {
		qDebug("ERROR: Could not create SSL bio.");
		return ret;
	}
	BIO_set_fd(ssl_bio, (int)(long)conn->clientSocktFD, BIO_NOCLOSE);

	SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_method());
	if (ssl_ctx == NULL) {
		qDebug("ERROR: Could not create SSL context.");
		BIO_free(ssl_bio);
		return ret;
	}

#if OPENSSL_VERSION_NUMBER < 0x10100002L || \
	(defined(LIBRESSL_VERSION_NUMBER) && (LIBRESSL_VERSION_NUMBER < 0x2060000fL))
	/* force use of TLSv1 for older devices */
	if (connection->device->version < DEVICE_VERSION(10,0,0)) {
#ifdef SSL_OP_NO_TLSv1_1
		long opts = SSL_CTX_get_options(ssl_ctx);
		opts |= SSL_OP_NO_TLSv1_1;
#ifdef SSL_OP_NO_TLSv1_2
		opts |= SSL_OP_NO_TLSv1_2;
#endif
#ifdef SSL_OP_NO_TLSv1_3
		opts |= SSL_OP_NO_TLSv1_3;
#endif
		SSL_CTX_set_options(ssl_ctx, opts);
#endif
	}
#else
	SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_VERSION);
	if (m_devVersion < DEVICE_VERSION(10,0,0)) {
		SSL_CTX_set_max_proto_version(ssl_ctx, TLS1_VERSION);
	}
#endif

	BIO* membp;
	X509* rootCert = NULL;
	membp = BIO_new_mem_buf(root_cert.data, root_cert.size);
	PEM_read_bio_X509(membp, &rootCert, NULL, NULL);
	BIO_free(membp);
	if (SSL_CTX_use_certificate(ssl_ctx, rootCert) != 1) {
		qDebug("WARNING: Could not load RootCertificate");
	}
	X509_free(rootCert);
	free(root_cert.data);

	RSA* rootPrivKey = NULL;
	membp = BIO_new_mem_buf(root_privkey.data, root_privkey.size);
	PEM_read_bio_RSAPrivateKey(membp, &rootPrivKey, NULL, NULL);
	BIO_free(membp);
	if (SSL_CTX_use_RSAPrivateKey(ssl_ctx, rootPrivKey) != 1) {
		qDebug("WARNING: Could not load RootPrivateKey");
	}
	RSA_free(rootPrivKey);
	free(root_privkey.data);

	SSL *ssl = SSL_new(ssl_ctx);
	if (!ssl) {
		qDebug("ERROR: Could not create SSL object");
		BIO_free(ssl_bio);
		SSL_CTX_free(ssl_ctx);
		return ret;
	}
	SSL_set_connect_state(ssl);
	SSL_set_verify(ssl, 0, ssl_verify_callback);
	SSL_set_bio(ssl, ssl_bio, ssl_bio);

	HandshakeThread *ht = new HandshakeThread(ssl);
	connect(ht, &HandshakeThread::handshakeCompleted, this, [=, &return_me](quint32 result){
		return_me = result;
		m_handshakeBlockEvent->quit();
	});
	ht->start();
	m_handshakeBlockEvent->exec();
	ht->deleteLater();

	if (return_me != 1) {
		qDebug("ERROR in SSL_do_handshake: %s", ssl_error_to_string(SSL_get_error(ssl, return_me)));
		SSL_free(ssl);
		SSL_CTX_free(ssl_ctx);
	} else {
		ssl_data_t ssl_data_loc = (ssl_data_t)malloc(sizeof(struct ssl_data_private));
		ssl_data_loc->session = ssl;
		ssl_data_loc->ctx = ssl_ctx;
		conn->ssl_data = ssl_data_loc;
		ret = true;
		qDebug("SSL mode enabled, %s, cipher: %s", SSL_get_version(ssl), SSL_get_cipher(ssl));
	}
	/* required for proper multi-thread clean up to prevent leaks */
	openssl_remove_thread_state();
	return ret;
}

bool MirrorManager::lockdownDisableSSL(ConnectionInfo *conn)
{
	bool ret = true;
	if (!conn->ssl_data) {
		/* ignore if ssl is not enabled */
		return ret;
	}

	if (conn->ssl_data->session) {
		/* see: https://www.openssl.org/docs/ssl/SSL_shutdown.html#RETURN_VALUES */
		if (SSL_shutdown(conn->ssl_data->session) == 0) {
			/* Only try bidirectional shutdown if we know it can complete */
			int ssl_error;
			if ((ssl_error = SSL_get_error(conn->ssl_data->session, 0)) == SSL_ERROR_NONE) {
				SSL_shutdown(conn->ssl_data->session);
			} else  {
				qDebug("Skipping bidirectional SSL shutdown. SSL error code: %i\n", ssl_error);
			}
		}
	}

	internal_ssl_cleanup(conn->ssl_data);
	free(conn->ssl_data);
	conn->ssl_data = NULL;

	qDebug("SSL mode disabled");

	return ret;
}


bool MirrorManager::lockdownStartSession(ConnectionInfo *conn, const char *host_id, char **session_id, int *ssl_enabled)
{
	bool ret = false;
	plist_t dict = NULL;

	if (!host_id)
		ret = ret;

	/* if we have a running session, stop current one first */
	if (session_id) {
		if (!lockdownStopSession(conn, conn->sessionId))
			return ret;
	}

	/* setup request plist */
	dict = plist_new_dict();
	plist_dict_add_label(dict, "usbmuxd");
	plist_dict_set_item(dict,"Request", plist_new_string("StartSession"));

	/* add host id */
	if (host_id) {
		plist_dict_set_item(dict, "HostID", plist_new_string(host_id));
	}

	/* add system buid */
	char *system_buid = NULL;
	userpref_read_system_buid(&system_buid);
	if (system_buid) {
		plist_dict_set_item(dict, "SystemBUID", plist_new_string(system_buid));
		if (system_buid) {
			free(system_buid);
			system_buid = NULL;
		}
	}

	ret = sendPlist(conn, dict, false);
	plist_free(dict);
	dict = NULL;

	if (!ret)
		return ret;

	ret = receivePlist(conn, &dict, "StartSession");

	if (!ret || !dict)
		return ret;

	ret = lockdownCheckResult(dict, "StartSession");
	if (ret) {
		uint8_t use_ssl = 0;

		plist_t enable_ssl = plist_dict_get_item(dict, "EnableSessionSSL");
		if (enable_ssl && (plist_get_node_type(enable_ssl) == PLIST_BOOLEAN)) {
			plist_get_bool_val(enable_ssl, &use_ssl);
		}
		qDebug("Session startup OK");

		if (ssl_enabled != NULL)
			*ssl_enabled = use_ssl;

		/* store session id, we need it for StopSession */
		plist_t session_node = plist_dict_get_item(dict, "SessionID");
		if (session_node && (plist_get_node_type(session_node) == PLIST_STRING)) {
			plist_get_string_val(session_node, &conn->sessionId);
		}

		if (conn->sessionId) {
			qDebug("SessionID: %s", conn->sessionId);
			if (session_id != NULL)
				*session_id = strdup(conn->sessionId);
		} else {
			qDebug("Failed to get SessionID!");
		}

		qDebug("Enable SSL Session: %s", (use_ssl ? "true" : "false"));

		if (use_ssl) {
			ret = lockdownEnableSSL(conn);
			conn->sslEnabled = (ret ? 1 : 0);
		} else {
			ret = true;
			conn->sslEnabled = 0;
		}
	} else
		pairError(u8"startSession check result出错。");

	plist_free(dict);
	dict = NULL;

	return ret;
}

void MirrorManager::startActualPair(ConnectionInfo *conn)
{
	plist_t dict = plist_new_dict();
	plist_dict_add_label(dict, "usbmuxd");
	plist_dict_set_item(dict, "Request", plist_new_string("QueryType"));
	sendPlist(conn, dict, false);
	plist_free(dict);

	if (!receivePlist(conn, &dict, "QueryType"))
		return;

	plist_t type_node = plist_dict_get_item(dict, "Type");
	if (type_node && (plist_get_node_type(type_node) == PLIST_STRING)) {
		char* typestr = NULL;
		plist_get_string_val(type_node, &typestr);
		bool lockdown = strcmp(typestr, "com.apple.mobile.lockdown") != 0;
		qDebug("success with type %s", typestr);
		if (typestr)
			free(typestr);

		if (lockdown) {
			pairSuccess(conn);
		} else {
			if (config_has_device_record(m_serial)) {
				char *host_id = NULL;
				config_device_record_get_host_id(m_serial, &host_id);
				bool success = lockdownStartSession(conn, host_id, NULL, NULL);
				if (host_id)
					free(host_id);

				if (success)
					pairSuccess(conn);
				else {
					qDebug("%s: The stored pair record for device %s is invalid. Removing.", __func__, m_serial);
					if (config_remove_device_record(m_serial) == 0) {
						startConnect(InitPair, 0xf27e);
					} else {
						qDebug("%s: Could not remove pair record for device %s", __func__, m_serial);
						pairError(u8"无法删除之前的配对记录，请重启电脑后再试。");
					}
				}
			} else {	
				plist_t value = NULL;
				if (lockdownGetValue(conn, NULL, "ProductVersion", &value))
					handleVersionValue(conn, value);			
			}
		}
	} else {
		qDebug("hmm. QueryType response does not contain a type?!");
		pairError(u8"QueryType响应中未包含Type字段。");
	}

	plist_free(dict);
}

void MirrorManager::pairSuccess(ConnectionInfo *conn)
{
	qDebug() << "enter pair success";

	if (conn->sessionId)
		m_devicePaired = lockdownStopSession(conn, conn->sessionId) && m_deviceHandle;
	else 
		m_devicePaired = true;

	if (m_devicePaired) {
		clearPairResource();
		m_pairBlockEvent->quit();
	}

	qDebug() << "leave pair success";
}

void MirrorManager::startFinalPair(ConnectionInfo *conn)
{
	if(lockdownPair(conn, NULL, true))
		pairSuccess(conn);
}

void MirrorManager::lockdownProcessNotification(const char *notification)
{
	if (!notification || strlen(notification) == 0) {
		pairError(u8"配对返回的notification字符串有误。");
		return;
	}

	if (strcmp(notification, "com.apple.mobile.lockdown.request_pair") == 0) {
		qDebug("%s: user trusted this computer on device %s, pairing now", __func__, m_serial);
		startConnect(PairNotify, 0xf27e);
	} else if (strcmp(notification, "com.apple.mobile.lockdown.request_host_buid") == 0) {
		startConnect(Other, 0xf27e);
	}
}

void MirrorManager::startObserve(ConnectionInfo *conn)
{
	const char* spec[] = {
			"com.apple.mobile.lockdown.request_pair",
			"com.apple.mobile.lockdown.request_host_buid",
			NULL
		};

	auto func = [conn, this](const char *notification){
		plist_t dict = plist_new_dict();
		plist_dict_set_item(dict,"Command", plist_new_string("ObserveNotification"));
		plist_dict_set_item(dict,"Name", plist_new_string(notification));
		sendPlist(conn, dict, false);
		plist_free(dict);
	};

	int i = 0;
	while (spec[i]) {
		func(spec[i]);
		i++;
	}

	m_notificationTimer->disconnect();
	connect(m_notificationTimer, &QTimer::timeout, this, [conn, this](){
		if (!m_connections.contains(conn))
			return;

		plist_t dict = NULL;

		bool success = receivePlist(conn, &dict, "GetNotification", true); //data->plist 可能失败吗？这里假设失败的原因就是超时
		if (!success || !dict)
			return;

		m_notificationTimer->stop();

		int res = 0;
		char *cmd_value = NULL;
		char *notification = NULL;
		plist_t cmd_value_node = plist_dict_get_item(dict, "Command");

		if (plist_get_node_type(cmd_value_node) == PLIST_STRING) {
			plist_get_string_val(cmd_value_node, &cmd_value);
		}

		if (cmd_value && !strcmp(cmd_value, "RelayNotification")) {
			char *name_value = NULL;
			plist_t name_value_node = plist_dict_get_item(dict, "Name");

			if (plist_get_node_type(name_value_node) == PLIST_STRING) {
				plist_get_string_val(name_value_node, &name_value);
			}

			res = -2;
			if (name_value_node && name_value) {
				notification = name_value;
				qDebug("got notification %s", __func__, name_value);
				res = 0;
			}
		} else if (cmd_value && !strcmp(cmd_value, "ProxyDeath")) {
			qDebug("NotificationProxy died!");
			pairError(u8"配对监听代理失效。");
			res = -1;
		} else if (cmd_value) {
			qDebug("unknown NotificationProxy command '%s' received!", cmd_value);
			pairError(QString(u8"未知的配对监听代理指令%1。").arg(cmd_value));
			res = -1;
		} else {
			res = -2;
			pairError(u8"未知的配对监听错误。");
		}

		if (res == 0)
			lockdownProcessNotification(notification);

		if (cmd_value) {
			free(cmd_value);
		}
		if (notification) {
			free(notification);
		}
		plist_free(dict);
		dict = NULL;

	});
	m_notificationTimer->start(200);
}

void MirrorManager::onDeviceTcpInput(struct tcphdr *th, unsigned char *payload, uint32_t payload_length)
{
	uint16_t sport = ntohs(th->th_dport);
	uint16_t dport = ntohs(th->th_sport);

	ConnectionInfo *conn = NULL;
	for (auto iter = m_connections.begin(); iter != m_connections.end(); iter++) {
		ConnectionInfo *i = *iter;
		if(i->srcPort == sport && i->dstPort == dport) {
			conn = i;
			break;
		}
	}

	if(!conn) {
		if(!(th->th_flags & TH_RST)) {
			qDebug("No connection for device incoming packet %d->%d", dport, sport);
			if(sendAnonRst(sport, dport, ntohl(th->th_seq)) < 0)
				qDebug("Error sending TCP RST to device (%d->%d)", sport, dport);
		}
		return;
	}

	if(th->th_flags & TH_RST) {
		char *buf = (char *)malloc(payload_length+1);
		memcpy(buf, payload, payload_length);
		if(payload_length && (buf[payload_length-1] == '\n'))
			buf[payload_length-1] = 0;
		buf[payload_length] = 0;
		qDebug("RST reason: %s", buf);
		free(buf);
	}

	if(conn->connState == MuxConnState::CONN_CONNECTING) {
		if(th->th_flags != (TH_SYN|TH_ACK)) {
			if(th->th_flags & TH_RST)
				conn->connState = MuxConnState::CONN_REFUSED;
			qDebug("Connection refused (%d->%d)", sport, dport);

			removeConnection(conn);
		} else {
			conn->connTxSeq++;
			conn->connTxAck++;
			if(sendTcp(conn, TH_ACK, NULL, 0) < 0) {
				qDebug("Error sending TCP ACK to device (%d->%d)", sport, dport);

				removeConnection(conn);
				return;
			}
			conn->connState = MuxConnState::CONN_CONNECTED;

			if (conn->dstPort == 0xf27e) {
				switch (conn->type)
				{
				case InitPair:
					startActualPair(conn);
					break;
				case PairNotify:
					startFinalPair(conn);
					break;
				case Other:
					setUntrustedHostBuid(conn);
					break;
				case StartService:
					break;
				default:
					break;
				}
			} else
				startObserve(conn);
		}
	} else if(conn->connState == MuxConnState::CONN_CONNECTED) {
		conn->connTxAck += payload_length;
		if(th->th_flags != TH_ACK) {
			qDebug("Connection reset by device (%d->%d)", sport, dport);
			if(th->th_flags & TH_RST)
				conn->connState = MuxConnState::CONN_DYING;

			removeConnection(conn);
		} else {
			if (conn->clientSocketInServerSide)
				conn->clientSocketInServerSide->write((char *)payload, payload_length);
			else {
				circlebuf_push_back(&conn->m_usbDataCache, payload, payload_length);
				m_usbDataBlockEvent->quit();
			}
			// Device likes it best when we are prompty ACKing data
			sendTcpAck(conn);
		}
	}
}

bool MirrorManager::readDataWithSize(ConnectionInfo *conn, void *dst, size_t size, bool allowTimeout, int timeout)
{
	bool ret = false;
	if (!m_connections.contains(conn))
		return ret;

	while (conn->m_usbDataCache.size < size) {
		QTimer timer;
		timer.setSingleShot(true);
		connect(&timer, &QTimer::timeout, m_usbDataBlockEvent, &QEventLoop::quit);
		timer.start(timeout);
		m_usbDataBlockEvent->exec();
		if (!m_connections.contains(conn))
			return false;

		if (!timer.isActive())
			break;
	}

	if (conn->m_usbDataCache.size >= size) {
		circlebuf_pop_front(&conn->m_usbDataCache, dst, size);
		ret = true;
	} else {
		ret = false;
		if (!allowTimeout)
			pairError(u8"读取usb数据超时。");
	}

	return ret;
}

enum fd_mode { FDM_READ, FDM_WRITE, FDM_EXCEPT };
typedef enum fd_mode fd_mode;
int socket_check_fd(int fd, fd_mode fdm, unsigned int timeout)
{
	fd_set fds;
	int sret;
	int eagain;
	struct timeval to;
	struct timeval *pto;

	if (fd < 0) {
		qDebug("ERROR: invalid fd in check_fd %d", fd);
		return -1;
	}

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	sret = -1;

	do {
		if (timeout > 0) {
			to.tv_sec = (time_t) (timeout / 1000);
			to.tv_usec = (time_t) ((timeout - (to.tv_sec * 1000)) * 1000);
			pto = &to;
		} else {
			pto = NULL;
		}
		eagain = 0;
		switch (fdm) {
		case FDM_READ:
			sret = select(fd + 1, &fds, NULL, NULL, pto);
			break;
		case FDM_WRITE:
			sret = select(fd + 1, NULL, &fds, NULL, pto);
			break;
		case FDM_EXCEPT:
			sret = select(fd + 1, NULL, NULL, &fds, pto);
			break;
		default:
			return -1;
		}

		if (sret < 0) {
			switch (errno) {
			case EINTR:
				// interrupt signal in select
				qDebug("%s: EINTR\n", __func__);
				eagain = 1;
				break;
			case EAGAIN:
				qDebug("%s: EAGAIN\n", __func__);
				break;
			default:
				qDebug("%s: select failed: %s\n", __func__,
				       strerror(errno));
				return -1;
			}
		} else if (sret == 0) {
			qDebug("%s: timeout", __func__);
			return -ETIMEDOUT;
		}
	} while (eagain);

	return sret;
}

int socket_recv_to_idevice_error(int conn_error, uint32_t len, uint32_t received)
{
	if (conn_error < 0) {
		switch (conn_error) {
		case -EAGAIN:
			qDebug("ERROR: received partial data %d/%d (%s)",
				   received, len, strerror(-conn_error));
			return -1;
		case -ETIMEDOUT:
			return -2;
		default:
			return -3;
		}
	}

	return 0;
}

bool MirrorManager::readDataFromSSL(ConnectionInfo *conn, void *dst, size_t size, int timeout)
{
	bool ret = false;
	if (!m_connections.contains(conn))
		return ret;

	QEventLoop block;
	auto read = [=, &block, &ret](){
		QMutexLocker locker(&conn->deleteMutex);
		if (!conn->ssl_data)
			return;

		uint32_t received = 0;
		int do_select = 1;
		while (received < size) {
			do_select = (SSL_pending(conn->ssl_data->session) == 0);
			if (do_select) {
				int conn_error = socket_check_fd((int)(long)conn->clientSocktFD, FDM_READ, timeout);
				auto error = socket_recv_to_idevice_error(conn_error, size, received);

				switch (error) {
					case 0:
						break;
					case -3:
						qDebug("ERROR: socket_check_fd returned %d (%s)", conn_error, strerror(-conn_error));
					default:
						qDebug("ERROR: socket_check_fd returned unknown");
				}

				if (error != 0) {
					ret = false;
					block.quit();
					return;
				}
			}

			int r = SSL_read(conn->ssl_data->session, (void*)((char*)dst+received), (int)size-received);
			if (r > 0) {
				received += r;
			} else {
				break;
			}
		}

		ret = received >= size;
		block.quit();
	};
	std::thread th(read);
	block.exec();
	if (th.joinable())
		th.join();

	return ret;
}

bool MirrorManager::readAllData(ConnectionInfo *conn, void **dst, size_t *outSize, int timeout /* = 500 */)
{
	bool ret = false;
	if (!m_connections.contains(conn))
		return ret;

	while (conn->m_usbDataCache.size <= 0) {
		QTimer timer;
		timer.setSingleShot(true);
		connect(&timer, &QTimer::timeout, m_usbDataBlockEvent, &QEventLoop::quit);
		timer.start(timeout);
		m_usbDataBlockEvent->exec();
		if (!m_connections.contains(conn))
			return false;

		if (!timer.isActive())
			break;
	}

	if (conn->m_usbDataCache.size > 0) {
		*outSize = conn->m_usbDataCache.size;
		*dst = malloc(*outSize);
		circlebuf_pop_front(&conn->m_usbDataCache, *dst, *outSize);
		ret = true;
	} else {
		ret = false;
		pairError(u8"读取usb数据超时。");
	}

	return ret;
}

void MirrorManager::onDeviceData(QByteArray data)
{
	int length = data.length();
	auto buffer = data.data();
	unsigned char *buffer2 = NULL;
	if (!length)
		return;

	// sanity check (should never happen with current USB implementation)
	if ((length > USB_MRU) || (length > DEV_MRU)) {
		qDebug() << "Too much data received from USB, file a bug";
		return;
	}

	// handle broken up transfers
	if (m_pktlen) {
		if ((length + m_pktlen) > DEV_MRU) {
			qDebug("Incoming split packet is too large (%d so far), dropping!", length + m_pktlen);
			m_pktlen = 0;
			return;
		}
		memcpy(m_pktbuf + m_pktlen, buffer, length);
		MuxHeader *mhdr = (MuxHeader*)m_pktbuf;
		if ((length < USB_MRU) || (ntohl(mhdr->length) == (length + m_pktlen))) {
			buffer2 = m_pktbuf;
			length += m_pktlen;
			m_pktlen = 0;
			qDebug("Gathered mux data from buffer (total size: %d)", length);
		} else {
			m_pktlen += length;
			qDebug("Appended mux data to buffer (total size: %d)", m_pktlen);
			return;
		}
	} else {
		MuxHeader *mhdr = (MuxHeader *)buffer;
		if ((length == USB_MRU) && (length < ntohl(mhdr->length))) {
			memcpy(m_pktbuf, buffer, length);
			m_pktlen = length;
			qDebug("Copied mux data to buffer (size: %d)", m_pktlen);
			return;
		}
	}

	MuxHeader *mhdr = (buffer2 == NULL ? (MuxHeader *)buffer : (MuxHeader *)buffer2);
	int mux_header_size = ((m_devVersion < 2) ? 8 : sizeof(MuxHeader));
	if (ntohl(mhdr->length) != length) {
		qDebug("Incoming packet size mismatch (expected %d, got %d)", ntohl(mhdr->length), length);
		return;
	}

	struct tcphdr *th;
	unsigned char *payload;
	uint32_t payload_length;

	if (m_devVersion >= 2) {
		m_rxSeq = ntohs(mhdr->rx_seq);
	}

	MuxProtocol protoc = (MuxProtocol)ntohl(mhdr->protocol);
	switch (protoc) {
	case MuxProtocol::MUX_PROTO_VERSION:
		if (length < (mux_header_size + sizeof(VersionHeader))) {
			qDebug("Incoming version packet is too small (%d)", length);
			return;
		}
		onDeviceVersionInput((VersionHeader *)((char *)mhdr + mux_header_size));
		break;
	case MuxProtocol::MUX_PROTO_CONTROL:
		payload = (unsigned char *)(mhdr + 1);
		payload_length = length - mux_header_size;
		break;
	case MuxProtocol::MUX_PROTO_TCP:
		if (length < (mux_header_size + sizeof(struct tcphdr))) {
			qDebug("Incoming TCP packet is too small (%d)", length);
			return;
		}
		th = (struct tcphdr *)((char *)mhdr + mux_header_size);
		payload = (unsigned char *)(th + 1);
		payload_length = length - sizeof(struct tcphdr) - mux_header_size;
		onDeviceTcpInput(th, payload, payload_length);
		break;
	default:
		qDebug("Incoming packet has unknown protocol 0x%x)", ntohl(mhdr->protocol));
		break;
	}
}

bool MirrorManager::sendPlist(ConnectionInfo *conn, plist_t plist, bool isBinary)
{
	if (!m_connections.contains(conn))
		return false;

	char *content = NULL;
	uint32_t total = 0;
	uint32_t length = 0;
	uint32_t nlen = 0;

	if (!plist) {
		return false;
	}

	if (isBinary) {
		plist_to_bin(plist, &content, &length);
	} else {
		plist_to_xml(plist, &content, &length);
	}

	if (!content || length == 0) {
		return false;
	}

	nlen = htobe32(length);
	qDebug("sending %d bytes", length);

	total = length + sizeof(nlen);
	char *sendBuffer = (char *)malloc(total);
	memcpy(sendBuffer, &nlen, sizeof(nlen));
	memcpy(sendBuffer + sizeof(nlen), content, length);

	if (conn->ssl_data) {
		uint32_t sent = 0;
		while (sent < total) {
			int s = SSL_write(conn->ssl_data->session, (const void*)(sendBuffer + sent), (int)(total - sent));
			if (s < 0) {
				break;
			}
			sent += s;
		}
	} else
		sendTcp(conn, TH_ACK, (const unsigned char *)sendBuffer, total);

	free(content);
	free(sendBuffer);

	return true;
}

int MirrorManager::sendAnonRst(uint16_t sport, uint16_t dport, uint32_t ack)
{
	struct tcphdr th;
	memset(&th, 0, sizeof(th));
	th.th_sport = htons(sport);
	th.th_dport = htons(dport);
	th.th_ack = htonl(ack);
	th.th_flags = TH_RST;
	th.th_off = sizeof(th) / 4;

	qDebug("[OUT] sport=%d dport=%d flags=0x%x", sport, dport, th.th_flags);

	int res = sendPacket(MuxProtocol::MUX_PROTO_TCP, &th, NULL, 0);
	return res;
}

bool MirrorManager::receivePlistInternal(char *content, uint32_t pktlen, plist_t *plist)
{
	if (!plist)
		return false;

	*plist = NULL;

	if ((pktlen > 8) && !memcmp(content, "bplist00", 8)) {
		plist_from_bin(content, pktlen, plist);
	} else if ((pktlen > 5) && !memcmp(content, "<?xml", 5)) {
		/* iOS 4.3+ hack: plist data might contain invalid characters, thus we convert those to spaces */
		for (uint32_t bytes = 0; bytes < pktlen-1; bytes++) {
			if ((content[bytes] >= 0) && (content[bytes] < 0x20) && (content[bytes] != 0x09) && (content[bytes] != 0x0a) && (content[bytes] != 0x0d))
				content[bytes] = 0x20;
		}
		plist_from_xml(content, pktlen, plist);
	} else {
		qDebug("WARNING: received unexpected non-plist content");
	}
	
	return *plist != NULL;
}

static struct usb_device *findAppleDevice(int vid, int pid)
{
	usb_find_busses();
	usb_find_devices();

	struct usb_bus *bus;
	struct usb_device *dev;

	for (bus = usb_get_busses(); bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor != vid || dev->descriptor.idProduct != pid) {
				continue;
			}
			return dev;
		}
	}
	return nullptr;
}

#include <QThread>
bool MirrorManager::checkAndChangeMode(int vid, int pid)
{
	auto openDeviceFunc = [=](struct usb_device *dev, int index){
		m_qtConfigIndex = index;
		m_deviceHandle = usb_open(dev);
		m_device = dev;
		memset(m_serial, 0, sizeof(m_serial));
		usb_get_string_simple(m_deviceHandle, m_device->descriptor.iSerialNumber, m_serial, sizeof(m_serial));
		qDebug() << "open ios device";
	};

	bool ret = false;
	struct usb_device *device = findAppleDevice(vid, pid);
	if (!device) {
		m_errorMsg = u8"iOS设备未找到";
		return ret;
	}	

	int muxConfigIndex = -1;
	int qtConfigIndex = -1;
	findConfigurations(&device->descriptor, device, &muxConfigIndex, &qtConfigIndex);
	if (qtConfigIndex == -1) {
		qDebug() << "change Apple device index";
		auto deviceHandle = usb_open(device);
		if (!deviceHandle) {
			m_errorMsg = u8"iOS设备未找到， 请保持设备连接。";
			return ret;
		}
		usb_control_msg(deviceHandle, 0x40, 0x52, 0x00, 0x02, NULL, 0, 0);
		usb_close(deviceHandle);
		QThread::msleep(200);
		QElapsedTimer t;
		t.start();
		int loopCount = 0;
		while (true) {
			loopCount++;
			auto dev = findAppleDevice(vid, pid);
			if (!dev) {
				QThread::msleep(10);
				if(loopCount > 300) {
					m_errorMsg = u8"切换投屏模式过程中设备丢失连接，请确保设备与电脑有良好的连接。";
					break;
				}
				continue;
			} else {
				findConfigurations(&dev->descriptor, dev, &muxConfigIndex, &qtConfigIndex);
				if (qtConfigIndex == -1)
					m_errorMsg = u8"进入投屏模式失败，请断开连接，重启手机后再试。";
				else {
					openDeviceFunc(dev, qtConfigIndex);
					ret = true;
				}
				break;
			}
		}
		qDebug() << "Apple device reconnect cost time " << t.elapsed() << ", loop count: " << loopCount;
	} else {
		openDeviceFunc(device, qtConfigIndex);
		ret = true;
	}

	qDebug() << "checkAndChangeMode result: " << ret;
	return ret;
}

bool MirrorManager::setupUSBInfo()
{
	qDebug() << "enter setup usb";
	if(!m_device || !m_deviceHandle) {
		m_errorMsg = u8"iOS设备异常，请插拔后再试";
		return false;
	}

	char config = -1;
	int res = usb_control_msg(m_deviceHandle, USB_RECIP_DEVICE | USB_ENDPOINT_IN, USB_REQ_GET_CONFIGURATION, 0, 0, &config, 1, 500);
	if (res < 0) {
		m_errorMsg = u8"发送控制指令失败。";
		return false;
	}
	if (config != m_qtConfigIndex) {
		if (usb_set_configuration(m_deviceHandle, m_qtConfigIndex)) {
			m_errorMsg = u8"设置configuration失败。";
			return false;
		}
	}

	for (int m = 0; m < m_device->descriptor.bNumConfigurations; m++) {
		struct usb_config_descriptor config = m_device->config[m];
		for (int n = 0; n < config.bNumInterfaces; n++) {
			struct usb_interface_descriptor intf = config.interface[n].altsetting[0];
			if (intf.bInterfaceClass == INTERFACE_CLASS
				&& intf.bInterfaceSubClass == INTERFACE_SUBCLASS
				&& intf.bInterfaceProtocol == INTERFACE_PROTOCOL) {
				if (intf.bNumEndpoints != 2) {
					continue;
				}
				if ((intf.endpoint[0].bEndpointAddress & 0x80) == USB_ENDPOINT_OUT
					&& (intf.endpoint[1].bEndpointAddress & 0x80) == USB_ENDPOINT_IN) {
					m_interface = intf.bInterfaceNumber;
					ep_out = intf.endpoint[0].bEndpointAddress;
					ep_in = intf.endpoint[1].bEndpointAddress;
				} else if ((intf.endpoint[1].bEndpointAddress & 0x80) == USB_ENDPOINT_OUT
					&& (intf.endpoint[0].bEndpointAddress & 0x80) == USB_ENDPOINT_IN) {
					m_interface = intf.bInterfaceNumber;
					ep_out = intf.endpoint[1].bEndpointAddress;
					ep_in = intf.endpoint[0].bEndpointAddress;
				}
			}

			if (intf.bInterfaceClass == INTERFACE_CLASS && intf.bInterfaceSubClass == INTERFACE_QUICKTIMECLASS) {
				if (intf.bNumEndpoints != 2) {
					continue;
				}
				if ((intf.endpoint[0].bEndpointAddress & 0x80) == USB_ENDPOINT_OUT &&
					(intf.endpoint[1].bEndpointAddress & 0x80) == USB_ENDPOINT_IN) {
					m_interface_fa = intf.bInterfaceNumber;
					ep_out_fa = intf.endpoint[0].bEndpointAddress;
					ep_in_fa = intf.endpoint[1].bEndpointAddress;
				} else if (
					(intf.endpoint[1].bEndpointAddress & 0x80) == USB_ENDPOINT_OUT &&
					(intf.endpoint[0].bEndpointAddress & 0x80) == USB_ENDPOINT_IN) {
					m_interface_fa = intf.bInterfaceNumber;
					ep_out_fa = intf.endpoint[1].bEndpointAddress;
					ep_in_fa = intf.endpoint[0].bEndpointAddress;
				}
			}
		}
	}

	res = usb_claim_interface(m_deviceHandle, m_interface);
	res = usb_claim_interface(m_deviceHandle, m_interface_fa);
	if (res < 0) {
		m_errorMsg = u8"claim interface失败。";
		return false;
	}

	qDebug() << "setup usb device ok";
	return true;
}

bool MirrorManager::startPair()
{
	m_inPair = true;
	m_pktlen = 0;
	m_stop = false;
	m_usbReadTh = std::thread(readUSBData, this);

	m_devicePaired = false;
	m_devVersion = 0;
	m_devState = MuxDevState::MUXDEV_INIT;
	VersionHeader vh;
	vh.major = htonl(2);
	vh.minor = htonl(0);
	vh.padding = 0;
	sendPacket(MuxProtocol::MUX_PROTO_VERSION, &vh, NULL, 0);

	m_pairBlockEvent->exec();

	m_inPair = false;
	qDebug() << "pair end ===================== " << m_errorMsg << m_devicePaired;

	return m_devicePaired;
}

void MirrorManager::usbExtractFrame(QByteArray data)
{
	if (!data.size() || !mp)
		return;

	circlebuf_push_back(&mp->m_mirrorDataBuf, data.data(), data.size());
	while (true) {
		size_t byte_remain = mp->m_mirrorDataBuf.size;
		if (byte_remain < 4)
			break;

		unsigned char buff_len[4] = {0};
		circlebuf_peek_front(&mp->m_mirrorDataBuf, buff_len, 4);
		uint32_t len = byteutils_get_int(buff_len, 0);
		if (byte_remain >= len) {
			//有一帧
			unsigned char *temp_buf = (unsigned char *)malloc(len);
			circlebuf_pop_front(&mp->m_mirrorDataBuf, temp_buf, len);
			mirrorFrameReceived(temp_buf + 4, len - 4);
			free(temp_buf);
		} else
			break;
	}
}

void MirrorManager::handleSyncPacket(unsigned char *buf, uint32_t length)
{
	uint32_t type = byteutils_get_int(buf, 12);
	switch (type) {
	case CWPA: {
		qDebug("CWPA");
		struct SyncCwpaPacket cwpaPacket;
		int res = newSyncCwpaPacketFromBytes(buf, &cwpaPacket);
		CFTypeID clockRef = cwpaPacket.DeviceClockRef + 1000;

		mp->localAudioClock = NewCMClockWithHostTime(clockRef);
		mp->deviceAudioClockRef = cwpaPacket.DeviceClockRef;
		uint8_t *device_info;
		size_t device_info_len;
		list_t *hpd1_dict = CreateHpd1DeviceInfoDict();
		NewAsynHpd1Packet(hpd1_dict, &device_info, &device_info_len);
		list_destroy(hpd1_dict);
		qDebug("Sending ASYN HPD1");
		writeUBSData(ep_out_fa, (char *)device_info, device_info_len, 1000);

		uint8_t *cwpa_reply;
		size_t cwpa_reply_len;
		CwpaPacketNewReply(&cwpaPacket, clockRef, &cwpa_reply,
				   &cwpa_reply_len);
		qDebug("Send CWPA-RPLY {correlation:%p, clockRef:%p}", cwpaPacket.CorrelationID, clockRef);
		writeUBSData(ep_out_fa, (char *)cwpa_reply, cwpa_reply_len, 1000);
		free(cwpa_reply);

		qDebug("Sending ASYN HPD1");
		writeUBSData(ep_out_fa, (char *)device_info, device_info_len, 1000);
		free(device_info);

		uint8_t *hpa1;
		size_t hpa1_len;
		list_t *hpa1_dict = CreateHpa1DeviceInfoDict();
		NewAsynHpa1Packet(hpa1_dict, cwpaPacket.DeviceClockRef, &hpa1, &hpa1_len);
		list_destroy(hpa1_dict);
		qDebug("Sending ASYN HPA1");
		writeUBSData(ep_out_fa, (char *)hpa1, hpa1_len, 1000);
		free(hpa1);
	} break;
	case AFMT: {
		qDebug("AFMT");
		struct SyncAfmtPacket afmtPacket = {0};
		if (NewSyncAfmtPacketFromBytes(buf, length, &afmtPacket) == 0) { //处理音频格式
			mp->sendAudioInfo((uint32_t)afmtPacket.AudioStreamInfo.SampleRate, (speaker_layout)afmtPacket.AudioStreamInfo.ChannelsPerFrame);
		}

		uint8_t *afmt;
		size_t afmt_len;
		AfmtPacketNewReply(&afmtPacket, &afmt, &afmt_len);
		qDebug("Send AFMT-REPLY {correlation:%p}", afmtPacket.CorrelationID);
		writeUBSData(ep_out_fa, (char *)afmt, afmt_len, 1000);
		free(afmt);
	} break;
	case CVRP: {
		qDebug("CVRP");
		struct SyncCvrpPacket cvrp_packet = {0};
		NewSyncCvrpPacketFromBytes(buf, length, &cvrp_packet);

		if (cvrp_packet.Payload) {
			list_node_t *node;
			list_iterator_t *it = list_iterator_new(cvrp_packet.Payload, LIST_HEAD);
			while ((node = list_iterator_next(it))) {
				struct StringKeyEntry *entry = (struct StringKeyEntry *)node->val;
				if (entry->typeMagic == FormatDescriptorMagic) {
					struct FormatDescriptor *fd = (struct FormatDescriptor *)entry->children;
					mp->sampleRate = fd->AudioDescription.SampleRate;
					break;
				}
			}
			list_iterator_destroy(it);
		}

		const double EPS = 1e-6;
		if (fabs(mp->sampleRate - 0.f) > EPS)
			mp->sampleRate = 48000.0f;

		mp->needClockRef = cvrp_packet.DeviceClockRef;
		AsynNeedPacketBytes(mp->needClockRef, &mp->needMessage, &mp->needMessageLen);
		writeUBSData(ep_out_fa, (char *)mp->needMessage, mp->needMessageLen, 1000);

		CFTypeID clockRef2 = cvrp_packet.DeviceClockRef + 0x1000AF;
		uint8_t *send_data;
		size_t send_data_len;
		SyncCvrpPacketNewReply(&cvrp_packet, clockRef2, &send_data, &send_data_len);
		writeUBSData(ep_out_fa, (char *)send_data, send_data_len, 1000);
		free(send_data);
		clearSyncCvrpPacket(&cvrp_packet);
	} break;
	case OG: {
		struct SyncOgPacket og_packet = {0};
		NewSyncOgPacketFromBytes(buf, length, &og_packet);
		uint8_t *send_data;
		size_t send_data_len;
		SyncOgPacketNewReply(&og_packet, &send_data, &send_data_len);
		writeUBSData(ep_out_fa, (char *)send_data, send_data_len, 1000);
		free(send_data);
	} break;
	case CLOK: {
		qDebug("CLOCK");
		struct SyncClokPacket clock_packet = {0};
		NewSyncClokPacketFromBytes(buf, length, &clock_packet);
		CFTypeID clockRef = clock_packet.ClockRef + 0x10000;
		mp->clock = NewCMClockWithHostTime(clockRef);

		uint8_t *send_data;
		size_t send_data_len;
		SyncClokPacketNewReply(&clock_packet, clockRef, &send_data, &send_data_len);
		writeUBSData(ep_out_fa, (char *)send_data, send_data_len, 1000);
		free(send_data);
	} break;
	case TIME: {
		qDebug("TIME");
		struct SyncTimePacket time_packet = {0};
		NewSyncTimePacketFromBytes(buf, length, &time_packet);
		struct CMTime time_to_send = GetTime(&mp->clock);

		uint8_t *send_data;
		size_t send_data_len;
		SyncTimePacketNewReply(&time_packet, &time_to_send, &send_data, &send_data_len);
		writeUBSData(ep_out_fa, (char *)send_data, send_data_len, 1000);
		free(send_data);
	} break;
	case SKEW: {
		struct SyncSkewPacket skew_packet = {0};
		int res = NewSyncSkewPacketFromBytes(buf, length, &skew_packet);
		if (res < 0)
			qDebug("Error parsing SYNC SKEW packet");

		double skewValue = CalculateSkew(
			&mp->startTimeLocalAudioClock,
			&mp->lastEatFrameReceivedLocalAudioClockTime,
			&mp->startTimeDeviceAudioClock,
			&mp->lastEatFrameReceivedDeviceAudioClockTime);

		uint8_t *send_data;
		size_t send_data_len;
		SyncSkewPacketNewReply(&skew_packet, mp->sampleRate, &send_data, &send_data_len);
		writeUBSData(ep_out_fa, (char *)send_data, send_data_len, 1000);
		free(send_data);
	} break;
	case STOP: {
		struct SyncStopPacket stop_packet = {0};
		int res = NewSyncStopPacketFromBytes(buf, length, &stop_packet);
		if (res < 0)
			qDebug("Error parsing SYNC STOP packet");

		uint8_t *send_data;
		size_t send_data_len;
		SyncStopPacketNewReply(&stop_packet, &send_data,
				       &send_data_len);
		writeUBSData(ep_out_fa, (char *)send_data, send_data_len, 1000);
		free(send_data);
	} break;
	default:
		qDebug("received unknown sync packet type"); //stop
		break;
	}
}

void MirrorManager::handleAsyncPacket(unsigned char *buf, uint32_t length)
{
	uint32_t type = byteutils_get_int(buf, 12);
	switch (type) {
	case EAT: {
		mp->audioSamplesReceived++;
		struct AsynCmSampleBufPacket eatPacket = {0};
		bool success = NewAsynCmSampleBufPacketFromBytes(buf, length,
								 &eatPacket);

		if (!success) {
			qDebug("unknown eat");
		} else {
			if (!mp->firstAudioTimeTaken) {
				mp->startTimeDeviceAudioClock = eatPacket.CMSampleBuf.OutputPresentationTimestamp;
				mp->startTimeLocalAudioClock = GetTime(&mp->localAudioClock);
				mp->lastEatFrameReceivedDeviceAudioClockTime = eatPacket.CMSampleBuf.OutputPresentationTimestamp;
				mp->lastEatFrameReceivedLocalAudioClockTime = mp->startTimeLocalAudioClock;
				mp->firstAudioTimeTaken = true;
			} else {
				mp->lastEatFrameReceivedDeviceAudioClockTime = eatPacket.CMSampleBuf.OutputPresentationTimestamp;
				mp->lastEatFrameReceivedLocalAudioClockTime = GetTime(&mp->localAudioClock);
			}

			mp->sendData(&eatPacket.CMSampleBuf);

			if (mp->audioSamplesReceived % 100 == 0) {
				qDebug("RCV Audio Samples:%d", mp->audioSamplesReceived);
			}
			clearAsynCmSampleBufPacket(&eatPacket);
		}
	} break;
	case FEED: {
		struct AsynCmSampleBufPacket acsbp = {0};
		bool success = NewAsynCmSampleBufPacketFromBytes(buf, length, &acsbp);
		if (!success) {
			writeUBSData(ep_out_fa, (char *)mp->needMessage, mp->needMessageLen, 1000);
		} else {
			mp->videoSamplesReceived++;

			mp->sendData(&acsbp.CMSampleBuf);

			if (mp->videoSamplesReceived % 500 == 0) {
				qDebug("RCV Video Samples:%d ", mp->videoSamplesReceived);
				mp->videoSamplesReceived = 0;
			}
			writeUBSData(ep_out_fa, (char *)mp->needMessage, mp->needMessageLen, 1000);
			clearAsynCmSampleBufPacket(&acsbp);
		}
	} break;
	case SPRP: {
		qDebug("SPRP");
	} break;
	case TJMP:
		qDebug("TJMP");
		break;
	case SRAT:
		qDebug("SART");
		break;
	case TBAS:
		qDebug("TBAS");
		break;
	case RELS:
		qDebug("RELS");
		mp->quitBlockEvent.quit();
		break;
	default:
		qDebug("UNKNOWN");
		break;
	}
}

void MirrorManager::mirrorFrameReceived(unsigned char *buf, uint32_t len)
{
	uint32_t type = byteutils_get_int(buf, 0);
	switch (type) {
	case PingPacketMagic:
		if (!mp->firstPingPacket) {
			unsigned char cmd[] = {0x10, 0x00, 0x00, 0x00,
					       0x67, 0x6e, 0x69, 0x70,
					       0x01, 0x00, 0x00, 0x00,
					       0x01, 0x00, 0x00, 0x00};
			writeUBSData(ep_out_fa, (char *)cmd, 16, 1000);
			mp->firstPingPacket = true;
		}
		break;
	case SyncPacketMagic:
		handleSyncPacket(buf, len);
		break;
	case AsynPacketMagic:
		handleAsyncPacket(buf, len);
		break;
	default:
		break;
	}
}

void MirrorManager::readMirrorData(void *data)
{
	qDebug() << "enter readMirrorData";
	MirrorManager *manager = (MirrorManager *)data;
	usb_clear_halt(manager->m_deviceHandle, manager->ep_in_fa);
	usb_clear_halt(manager->m_deviceHandle, manager->ep_out_fa);

	unsigned char *read_buffer = (unsigned char *)malloc(DEV_MRU);
	int readLen = -1;
	manager->m_stop = false;
	while (!manager->m_stop) {
		readLen = usb_bulk_read(manager->m_deviceHandle, manager->ep_in_fa, (char *)read_buffer, DEV_MRU, 200);
		if (readLen < 0) {
			if (readLen != -116) {
				QMetaObject::invokeMethod(manager, "clearMirrorResource");
				break;
			}
		} else
			QMetaObject::invokeMethod(manager, "usbExtractFrame", Q_ARG(QByteArray, QByteArray((char *)read_buffer, readLen)));
	}
	free(read_buffer);
	qDebug() << "leave readMirrorData";
}

bool MirrorManager::startScreenMirror()
{
	m_inMirror = true;
	mp = new ScreenMirrorInfo(ipc_client);
	mp->sendStatus(true);
	m_readMirrorDataTh = std::thread(readMirrorData, this);
	return true;
}

void MirrorManager::stopScreenMirror()
{
	if (!m_inMirror || !m_deviceHandle)
		return;

	{
		uint8_t *d1;
		size_t d1_len;
		NewAsynHPA0(mp->deviceAudioClockRef, &d1, &d1_len);
		writeUBSData(ep_out_fa, (char *)d1, d1_len, 1000);
		free(d1);
	}

	{
		uint8_t *d1;
		size_t d1_len;
		NewAsynHPD0(&d1, &d1_len);
		writeUBSData(ep_out_fa, (char *)d1, d1_len, 1000);
		free(d1);
	}

	QTimer timer;
	timer.setSingleShot(true);
	connect(&timer, &QTimer::timeout, &mp->quitBlockEvent, &QEventLoop::quit);
	timer.start(1000);
	mp->quitBlockEvent.exec();

	if (m_deviceHandle) {
		{
			uint8_t *d1;
			size_t d1_len;
			NewAsynHPD0(&d1, &d1_len);
			writeUBSData(ep_out_fa, (char *)d1, d1_len, 1000);
			free(d1);
			qDebug("OK. Ready to release USB Device.");
		}

		usb_release_interface(m_deviceHandle, m_interface_fa);
		usb_control_msg(m_deviceHandle, 0x40, 0x52, 0x00, 0x00, NULL, 0, 1000 /* LIBUSB_DEFAULT_TIMEOUT */);
		usb_set_configuration(m_deviceHandle, 4);
	}

	clearMirrorResource();
}

void MirrorManager::clearMirrorResource()
{
	if (!mp)
		return;

	qDebug() << "++++++++++++ enter clearMirrorResource";
	mp->sendStatus(false);

	m_inMirror = false;
	m_stop = true;
	if (m_readMirrorDataTh.joinable())
		m_readMirrorDataTh.join();

	delete mp;
	mp = nullptr;

	qDebug() << "++++++++++++ leave clearMirrorResource";
}

//-1=>没找到设备
//-2=>发送切换指令后，设备触发了重启，但是重启后还是没有启用新USB功能，提示用户重启手机
//-3=>设备已经在启用了投屏能力，提示用户插拔手机
//-4=>发送切换指令后，3s内未能重新找到设备，提示用户保持手机与电脑的连接
bool MirrorManager::startMirrorTask(int vid, int pid) 
{
	m_errorMsg.clear();
	if (checkAndChangeMode(vid, pid) && setupUSBInfo() && startPair()) {
		return startScreenMirror();
	}
	return false;
}

int MirrorManager::sendTcpAck(ConnectionInfo *conn)
{
	if (sendTcp(conn, TH_ACK, NULL, 0) < 0) {
		qDebug("Error sending TCP ACK");
		pairError(u8"发送响应指令失败，请重新连接。");
		return -1;
	}

	return 0;
}

int MirrorManager::sendTcp(ConnectionInfo *conn, uint8_t flags, const unsigned char *data, int length)
{
	struct tcphdr th;
	memset(&th, 0, sizeof(th));
	th.th_sport = htons(conn->srcPort);
	th.th_dport = htons(conn->dstPort);
	th.th_seq = htonl(conn->connTxSeq);
	th.th_ack = htonl(conn->connTxAck);
	th.th_flags = flags;
	th.th_off = sizeof(th) / 4;
	th.th_win = htons(131072 >> 8);

	auto ret = sendPacket(MuxProtocol::MUX_PROTO_TCP, &th, data, length);
	conn->connTxSeq += length;
	return ret;
}

int MirrorManager::writeUBSData(int ep, char *bytes, int size, int timeout)
{
	if (!m_deviceHandle)
		return -1;

	return usb_bulk_write(m_deviceHandle, ep, bytes, size, timeout);
}

int MirrorManager::sendPacket(MuxProtocol proto, void *header, const void *data, int length)
{
	if (!m_deviceHandle)
		return -1;

	char *buffer;
	int hdrlen;
	int res;

	switch(proto) {
		case MuxProtocol::MUX_PROTO_VERSION:
			hdrlen = sizeof(VersionHeader);
			break;
		case MuxProtocol::MUX_PROTO_SETUP:
			hdrlen = 0;
			break;
		case MuxProtocol::MUX_PROTO_TCP:
			hdrlen = sizeof(struct tcphdr);
			break;
		default:
			qDebug() << "Invalid protocol %d for outgoing packet";
			return -1;
	}
	int mux_header_size = ((m_devVersion < 2) ? 8 : sizeof(MuxHeader));

	int total = mux_header_size + hdrlen + length;
	
	if(total > USB_MTU) {
		qDebug() << "Tried to send packet larger than USB MTU";
		return -1;
	}

	buffer = (char *)malloc(total);
	MuxHeader *mhdr = (MuxHeader *)buffer;
	mhdr->protocol = htonl((u_long)proto);
	mhdr->length = htonl(total);
	if (m_devVersion >= 2) {
		mhdr->magic = htonl(0xfeedface);
		if (proto == MuxProtocol::MUX_PROTO_SETUP) {
			m_txSeq = 0;
			m_rxSeq = 0xFFFF;
		}
		mhdr->tx_seq = htons(m_txSeq);
		mhdr->rx_seq = htons(m_rxSeq);
		m_txSeq++;
	}	

	memcpy(buffer + mux_header_size, header, hdrlen);
	if(data && length)
		memcpy(buffer + mux_header_size + hdrlen, data, length);

	res = writeUBSData(ep_out, buffer, total, 1000);
	free(buffer);
	if(res < 0) {
		qDebug("usb_send failed while sending packet (len %d) to device", total);
		return res;
	}
	return total;
}
