#include "MirrorManager.h"
#include "tcp.h"
#include "endianness.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QTimer>

#define LOCKDOWN_PROTOCOL_VERSION "2"

MirrorManager::MirrorManager()
{
	usb_init();
	m_pktbuf = (unsigned char *)malloc(DEV_MRU);
	m_pairBlockEvent = new QEventLoop(this);
	m_usbDataBlockEvent = new QEventLoop(this);
	m_notificationTimer = new QTimer(this);
}

MirrorManager::~MirrorManager()
{
	free(m_pktbuf);
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
				QMetaObject::invokeMethod(manager, "resetDevice", Q_ARG(QString, u8"停止USB读取线程"), Q_ARG(bool ,false));
				break;
			}
		} else
			QMetaObject::invokeMethod(manager, "onDeviceData", Q_ARG(QByteArray, QByteArray(buffer, readLen)));
	}

	free(buffer);

	qDebug() << "pair read usb data thread stopped...";
}

void MirrorManager::resetDevice(QString msg, bool closeDevice)
{
	m_stop = true;
	if (m_usbReadTh.joinable())
		m_usbReadTh.join();

	m_errorMsg = msg;
	if (m_notificationTimer->isActive())
		m_notificationTimer->stop();

	qDeleteAll(m_connections);
	m_connections.clear();

	if (closeDevice && m_deviceHandle) {
		usb_release_interface(m_deviceHandle, m_interface);
		usb_release_interface(m_deviceHandle, m_interface_fa);
		usb_close(m_deviceHandle);
		m_deviceHandle = NULL;
		m_device = NULL;
	}

	m_usbDataBlockEvent->quit();
	m_pairBlockEvent->quit();
	qDebug() << "reset device called+++++++++++++++++++++";
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
	if (!readDataWithSize(conn, &pktlen, sizeof(pktlen), allowTimeout)) {
		qDebug() << QString("read %1 length failed").arg(key);
		return false;
	}

	pktlen = be32toh(pktlen);
	char *buf = (char *)malloc(pktlen);
	if (!readDataWithSize(conn, buf, pktlen, allowTimeout)) {
		qDebug() << QString("read %1 data failed").arg(key);
		free(buf);
		return false;
	}

	*ret = NULL;
	bool success = receivePlistInternal(buf, pktlen, ret);
	free(buf);
	if (!success) {
		resetDevice(QString(u8"%1 响应数据解析异常。").arg(key));
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
			resetDevice(QString(u8"GetValue 返回中没有 %1").arg(key));

	} else {
		resetDevice(u8"GetValue 返回校验失败。");
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
		resetDevice(u8"GetValue 返回校验失败。");

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
			resetDevice(u8"传递的配对信息缺少必要信息。");
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

				resetDevice(u8"生成配对信息有误。");
				return ret;
			}

			/* get wifi mac now, if we get it later we fail on iOS 7 which causes a reconnect */
			lockdownGetValue(conn, NULL, "WiFiAddress", &wifi_node);
		} else {
			/* use existing pair record */
			userpref_read_pair_record(m_serial, &pair_record_plist);
			if (!pair_record_plist) {
				resetDevice(u8"读取保存的host id有误。");
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
				free(value);
				if (raiseError)
					resetDevice(QString(u8"配对失败, %1").arg(value));
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
			resetDevice(u8"从配对记录中读取信息失败。");
			return false;
		}

		/* try to read the escrow bag from the record */
		plist_t escrow_bag = plist_dict_get_item(pair_record, USERPREF_ESCROW_BAG_KEY);
		if (!escrow_bag || (PLIST_DATA != plist_get_node_type(escrow_bag))) {
			qDebug("ERROR: Failed to retrieve the escrow bag from the device's record");
			plist_free(dict);
			plist_free(pair_record);
			resetDevice(u8"ERROR: Failed to retrieve the escrow bag from the device's record 从配对记录中读取信息失败。");
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
		resetDevice(u8"lockdownDoStartService 参数有误。");
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
		resetDevice(u8"StartService返回检查失败。");
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
			resetDevice(u8"iOS版本太低，投屏所需最低版本为iOS7。");
		else {
			qDebug("%s: Found ProductVersion %s device %s", __func__, versionStr, m_serial);
			setUntrustedHostBuid(conn);

			if (!m_devicePaired) {
				if (lockdownPair(conn, NULL)) {
					//干成功的事儿
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
		}

		free(versionStr);
	} else
		resetDevice(u8"ProductionVersion 获取version字符串失败。");

	plist_free(value);
}

bool MirrorManager::lockdownStopSession(ConnectionInfo *conn, const char *session_id)
{
	if (!session_id) {
		qDebug("no session_id given, cannot stop session");
		return false;
	}

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

	if (!dict) {
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
		//property_list_service_disable_ssl(client->parent);
		conn->sslEnabled = false;
	}

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
		lockdownStopSession(conn, conn->sessionId);
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

	if (!dict)
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
			ret = lockdownd_error(property_list_service_enable_ssl(client->parent));
			conn->sslEnabled = (ret ? 1 : 0);
		} else {
			ret = true;
			conn->sslEnabled = 0;
		}
	}

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
		qDebug("success with type %s", typestr);

		if (strcmp(typestr, "com.apple.mobile.lockdown") != 0) {
			int cc = 0;
			cc++; 
		} else {
			char *host_id = NULL;
			if (config_has_device_record(m_serial)) {
				config_device_record_get_host_id(m_serial, &host_id);
				lockdownStartSession(conn, host_id, NULL, NULL);
				//lerr = lockdownd_start_session(lockdown, host_id, NULL, NULL);
				//if (host_id)
				//	free(host_id);
				//if (lerr == LOCKDOWN_E_SUCCESS) {
				//	usbmuxd_log(LL_INFO, "%s: StartSession success for device %s", __func__, _dev->udid);
				//	usbmuxd_log(LL_INFO, "%s: Finished preflight on device %s", __func__, _dev->udid);
				//	client_device_add(info);
				//	goto leave;
				//}

				//usbmuxd_log(LL_INFO, "%s: StartSession failed on device %s, lockdown error %d", __func__, _dev->udid, lerr);
			} else {
								
			}
			free(host_id);

			plist_t value = NULL;
			if (lockdownGetValue(conn, NULL, "ProductVersion", &value))
				handleVersionValue(conn, value);
		}
		free(typestr);
	} else {
		qDebug("hmm. QueryType response does not contain a type?!");
		resetDevice(u8"QueryType响应中未包含Type字段。");
	}

	plist_free(dict);
}

void MirrorManager::pairResult(bool success)
{
	if (success) {
		m_devicePaired = true;
		m_stop = true;
		if (m_usbReadTh.joinable())
			m_usbReadTh.join();

		m_pairBlockEvent->quit();
	}
}

void MirrorManager::startFinalPair(ConnectionInfo *conn)
{
	pairResult(lockdownPair(conn, NULL, true));
}

void MirrorManager::lockdownProcessNotification(const char *notification)
{
	if (!notification || strlen(notification) == 0) {
		resetDevice(u8"配对返回的notification字符串有误。");
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
			resetDevice(u8"配对监听代理失效。");
			res = -1;
		} else if (cmd_value) {
			qDebug("unknown NotificationProxy command '%s' received!", cmd_value);
			resetDevice(QString(u8"未知的配对监听代理指令%1。").arg(cmd_value));
			res = -1;
		} else {
			res = -2;
			resetDevice(u8"未知的配对监听错误。");
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

//m_connTxAck 这个值和原版的投屏进程还不太一样，设置的时机不一样。这里我设置的早了。明天得看下这个值设置的早了会不会有什么问题

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
			resetDevice(u8"设备拒绝连接。"); //this also sends the notification to the client
		} else {
			conn->connTxSeq++;
			conn->connTxAck++;
			if(sendTcp(conn, TH_ACK, NULL, 0) < 0) {
				qDebug("Error sending TCP ACK to device (%d->%d)", sport, dport);
				resetDevice(u8"发送响应指令失败，请重新连接。");
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
			resetDevice(u8"设备重置了连接，请断开设备、重启后再试。");
		} else {
			circlebuf_push_back(&conn->m_usbDataCache, payload, payload_length);
			m_usbDataBlockEvent->quit();
			// Device likes it best when we are prompty ACKing data
			sendTcpAck(conn);
		}
	}
}

bool MirrorManager::readDataWithSize(ConnectionInfo *conn, void *dst, size_t size, bool allowTimeout, int timeout)
{
	bool ret = false;
	if (m_connections.isEmpty())
		return ret;

	while (conn->m_usbDataCache.size < size) {
		QTimer timer;
		timer.setSingleShot(true);
		connect(&timer, &QTimer::timeout, m_usbDataBlockEvent, &QEventLoop::quit);
		timer.start(timeout);
		m_usbDataBlockEvent->exec();
		if (m_connections.isEmpty())
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
			resetDevice(u8"读取usb数据超时。");
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
		//device_control_input(dev, payload, payload_length);
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
	if (m_connections.isEmpty())
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

	sendTcp(conn, TH_ACK, (const unsigned char *)sendBuffer, total);

	free(content);
	free(sendBuffer);

	conn->connTxSeq += total;

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

	int ret = false;
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
	//	m_errorMsg = u8"设备异常，请重新插拔手机后再试。";
	}

	qDebug() << "checkAndChangeMode result: " << ret;
	return ret;
}

bool MirrorManager::setupUSBInfo()
{
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
	return true;
}

bool MirrorManager::startPair()
{
	m_pktlen = 0;
	m_stop = false;
	m_usbReadTh = std::thread(readUSBData, this);
	m_usbReadTh.detach();

	m_devicePaired = false;
	m_devVersion = 0;
	m_devState = MuxDevState::MUXDEV_INIT;
	VersionHeader vh;
	vh.major = htonl(2);
	vh.minor = htonl(0);
	vh.padding = 0;
	sendPacket(MuxProtocol::MUX_PROTO_VERSION, &vh, NULL, 0);

	m_pairBlockEvent->exec();

	qDebug() << "pair end ===================== " << m_errorMsg << m_devicePaired;

	return true;
}

//-1=>没找到设备
//-2=>发送切换指令后，设备触发了重启，但是重启后还是没有启用新USB功能，提示用户重启手机
//-3=>设备已经在启用了投屏能力，提示用户插拔手机
//-4=>发送切换指令后，3s内未能重新找到设备，提示用户保持手机与电脑的连接
bool MirrorManager::startMirrorTask(int vid, int pid) 
{
	return checkAndChangeMode(vid, pid) && setupUSBInfo() && startPair();
}

int MirrorManager::sendTcpAck(ConnectionInfo *conn)
{
	if (sendTcp(conn, TH_ACK, NULL, 0) < 0) {
		qDebug("Error sending TCP ACK");
		resetDevice(u8"发送响应指令失败，请重新连接。");
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

	return sendPacket(MuxProtocol::MUX_PROTO_TCP, &th, data, length);
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

	res = usb_bulk_write(m_deviceHandle, ep_out, buffer, total, 1000);
	free(buffer);
	if(res < 0) {
		qDebug("usb_send failed while sending packet (len %d) to device", total);
		return res;
	}
	return total;
}
