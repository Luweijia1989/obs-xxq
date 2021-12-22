#include "MirrorManager.h"
#include "tcp.h"
#include "endianness.h"
#include <QDebug>
#include <QElapsedTimer>

MirrorManager::MirrorManager()
{
	usb_init();
	m_pktbuf = (unsigned char *)malloc(DEV_MRU);
	circlebuf_init(&m_usbDataCache);
}

MirrorManager::~MirrorManager()
{
	free(m_pktbuf);
	circlebuf_free(&m_usbDataCache);
}

void MirrorManager::readUSBData(void *data)
{
	MirrorManager *manager = (MirrorManager *)data;
	char *buffer = (char *)malloc(DEV_MRU);
	int readLen = 0;
	while (!manager->m_stop) {
		readLen = usb_bulk_read(manager->m_deviceHandle, manager->ep_in, buffer, DEV_MRU, 200);
		if (readLen < 0) {
			if (readLen != -116)
				break;
		} else
			manager->onDeviceData((unsigned char *)buffer, readLen);
	}
	free(buffer);
}

void MirrorManager::resetDevice(QString msg)
{
	m_errorMsg = msg;
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

	m_connTxAck = 0;
	m_connTxSeq = 0;
	m_connState = MuxConnState::CONN_CONNECTING;
	sendTcp(TH_SYN, NULL, 0);
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

bool MirrorManager::receivePlist(plist_t *ret, const char *key)
{
	uint32_t pktlen = 0;
	if (!readDataWithSize(&pktlen, sizeof(pktlen))) {
		qDebug() << "read QueryType length failed";
		return false;
	}

	pktlen = be32toh(pktlen);
	char *buf = (char *)malloc(pktlen);
	if (!readDataWithSize(buf, pktlen)) {
		qDebug() << "read QueryType data failed";
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

bool MirrorManager::lockdownGetValue(const char *domain, const char *key, plist_t *value)
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
	sendPlist(dict, false);
	plist_free(dict);

	if (!receivePlist(&dict, key)) {
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

bool MirrorManager::lockdownSetValue(const char *domain, const char *key, plist_t value)
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
	sendPlist(dict, false);
	plist_free(dict);

	if (!receivePlist(&dict, key)) {
		return ret;
	}

	if (lockdownCheckResult(dict, "SetValue"))
		ret = true;
	else 
		resetDevice(u8"GetValue 返回校验失败。");

	plist_free(dict);
	return ret;
}

static plist_t lockdownd_pair_record_to_plist(MirrorManager::PairRecord *pair_record)
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

bool MirrorManager::lockdownPair(PairRecord *record)
{
	plist_t options = plist_new_dict();
	plist_dict_set_item(options, "ExtendedPairingErrors", plist_new_bool(1));

	bool ret = lockdownDoPair(record, "Pair", options, NULL);

	plist_free(options);

	return ret;
}

bool MirrorManager::lockdownGetDevicePublicKeyAsKeyData(key_data_t *public_key)
{
	bool ret = false;
	plist_t value = NULL;
	char *value_value = NULL;
	uint64_t size = 0;

	ret = lockdownGetValue(NULL, "DevicePublicKey", &value);
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

bool MirrorManager::lockdownPairRecordgenerate(plist_t *pair_record)
{
	bool ret = false;

	key_data_t public_key = { NULL, 0 };
	char* host_id = NULL;
	char* system_buid = NULL;

	/* retrieve device public key */
	ret = lockdownGetDevicePublicKeyAsKeyData(&public_key);
	if (!ret) {
		qDebug("device refused to send public key.");
		//goto leave;
	}
	qDebug("device public key follows:\n%.*s", public_key.size, public_key.data);

	*pair_record = plist_new_dict();

	/* generate keys and certificates into pair record */
	userpref_error_t uret = USERPREF_E_SUCCESS;
	uret = pair_record_generate_keys_and_certs(*pair_record, public_key);
	ret = uret == 1;

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

bool MirrorManager::lockdownDoPair(PairRecord *pair_record, const char *verb, plist_t options, plist_t *result)
{
	return true;
	//plist_t dict = NULL;
	//plist_t pair_record_plist = NULL;
	//plist_t wifi_node = NULL;
	//int pairing_mode = 0; /* 0 = libimobiledevice, 1 = external */

	//if (pair_record && pair_record->system_buid && pair_record->host_id) {
	//	/* valid pair_record passed? */
	//	if (!pair_record->device_certificate || !pair_record->host_certificate || !pair_record->root_certificate) {
	//		return false;
	//	}

	//	/* use passed pair_record */
	//	pair_record_plist = lockdownd_pair_record_to_plist(pair_record);

	//	pairing_mode = 1;
	//} else {
	//	/* generate a new pair record if pairing */
	//	if (!strcmp("Pair", verb)) {
	//		ret = pair_record_generate(client, &pair_record_plist);

	//		if (ret != LOCKDOWN_E_SUCCESS) {
	//			if (pair_record_plist)
	//				plist_free(pair_record_plist);
	//			return ret;
	//		}

	//		/* get wifi mac now, if we get it later we fail on iOS 7 which causes a reconnect */
	//		lockdownd_get_value(client, NULL, "WiFiAddress", &wifi_node);
	//	} else {
	//		/* use existing pair record */
	//		userpref_read_pair_record(client->udid, &pair_record_plist);
	//		if (!pair_record_plist) {
	//			return LOCKDOWN_E_INVALID_HOST_ID;
	//		}
	//	}
	//}

	//plist_t request_pair_record = plist_copy(pair_record_plist);

	///* remove stuff that is private */
	//plist_dict_remove_item(request_pair_record, USERPREF_ROOT_PRIVATE_KEY_KEY);
	//plist_dict_remove_item(request_pair_record, USERPREF_HOST_PRIVATE_KEY_KEY);

	///* setup pair request plist */
	//dict = plist_new_dict();
	//plist_dict_add_label(dict, client->label);
	//plist_dict_set_item(dict, "PairRecord", request_pair_record);
	//plist_dict_set_item(dict, "Request", plist_new_string(verb));
	//plist_dict_set_item(dict, "ProtocolVersion", plist_new_string(LOCKDOWN_PROTOCOL_VERSION));

	//if (options) {
	//	plist_dict_set_item(dict, "PairingOptions", plist_copy(options));
	//}

	///* send to device */
	//ret = lockdownd_send(client, dict);
	//plist_free(dict);
	//dict = NULL;

	//if (ret != LOCKDOWN_E_SUCCESS) {
	//	plist_free(pair_record_plist);
	//	if (wifi_node)
	//		plist_free(wifi_node);
	//	return ret;
	//}

	///* Now get device's answer */
	//ret = lockdownd_receive(client, &dict);

	//if (ret != LOCKDOWN_E_SUCCESS) {
	//	plist_free(pair_record_plist);
	//	if (wifi_node)
	//		plist_free(wifi_node);
	//	return ret;
	//}

	//if (strcmp(verb, "Unpair") == 0) {
	//	/* workaround for Unpair giving back ValidatePair,
	//	 * seems to be a bug in the device's fw */
	//	if (lockdown_check_result(dict, NULL) != LOCKDOWN_E_SUCCESS) {
	//		ret = LOCKDOWN_E_PAIRING_FAILED;
	//	}
	//} else {
	//	if (lockdown_check_result(dict, verb) != LOCKDOWN_E_SUCCESS) {
	//		ret = LOCKDOWN_E_PAIRING_FAILED;
	//	}
	//}

	///* if pairing succeeded */
	//if (ret == LOCKDOWN_E_SUCCESS) {
	//	debug_info("%s success", verb);
	//	if (!pairing_mode) {
	//		debug_info("internal pairing mode");
	//		if (!strcmp("Unpair", verb)) {
	//			/* remove public key from config */
	//			userpref_delete_pair_record(client->udid);
	//		} else {
	//			if (!strcmp("Pair", verb)) {
	//				/* add returned escrow bag if available */
	//				plist_t extra_node = plist_dict_get_item(dict, USERPREF_ESCROW_BAG_KEY);
	//				if (extra_node && plist_get_node_type(extra_node) == PLIST_DATA) {
	//					debug_info("Saving EscrowBag from response in pair record");
	//					plist_dict_set_item(pair_record_plist, USERPREF_ESCROW_BAG_KEY, plist_copy(extra_node));
	//				}

	//				/* save previously retrieved wifi mac address in pair record */
	//				if (wifi_node) {
	//					debug_info("Saving WiFiAddress from device in pair record");
	//					plist_dict_set_item(pair_record_plist, USERPREF_WIFI_MAC_ADDRESS_KEY, plist_copy(wifi_node));
	//					plist_free(wifi_node);
	//					wifi_node = NULL;
	//				}

	//				userpref_save_pair_record(client->udid, client->mux_id, pair_record_plist);
	//			}
	//		}
	//	} else {
	//		debug_info("external pairing mode");
	//	}
	//} else {
	//	debug_info("%s failure", verb);
	//	plist_t error_node = NULL;
	//	/* verify error condition */
	//	error_node = plist_dict_get_item(dict, "Error");
	//	if (error_node) {
	//		char *value = NULL;
	//		plist_get_string_val(error_node, &value);
	//		if (value) {
	//			/* the first pairing fails if the device is password protected */
	//			ret = lockdownd_strtoerr(value);
	//			free(value);
	//		}
	//	}
	//}

	//if (pair_record_plist) {
	//	plist_free(pair_record_plist);
	//	pair_record_plist = NULL;
	//}

	//if (wifi_node) {
	//	plist_free(wifi_node);
	//	wifi_node = NULL;
	//}

	//if (result) {
	//	*result = dict;
	//} else {
	//	plist_free(dict);
	//	dict = NULL;
	//}

	//return ret;
}

void MirrorManager::setUntrustedHostBuid()
{
	char* system_buid = NULL;
	config_get_system_buid(&system_buid);
	qDebug("%s: Setting UntrustedHostBUID to %s", __func__, system_buid);
	lockdownSetValue(NULL, "UntrustedHostBUID", plist_new_string(system_buid));
	free(system_buid);
}

void MirrorManager::handleVersionValue(plist_t value)
{
	if (value && plist_get_node_type(value) == PLIST_STRING) {
		char *versionStr = NULL;
		plist_get_string_val(value, &versionStr);
		int versionMajor = strtol(versionStr, NULL, 10);
		if (versionMajor < 7)
			resetDevice(u8"iOS版本太低，投屏所需最低版本为iOS7。");
		else {
			qDebug("%s: Found ProductVersion %s device %s", __func__, versionStr, m_serial);
			setUntrustedHostBuid();

			if (!m_devicePaired) {
				if (lockdownPair(NULL)) {

				} else {

				}
			}
		}

		free(versionStr);
	} else
		resetDevice(u8"ProductionVersion 获取version字符串失败。");

	plist_free(value);
}

void MirrorManager::startActualPair()
{
	plist_t dict = plist_new_dict();
	plist_dict_add_label(dict, "usbmuxd");
	plist_dict_set_item(dict, "Request", plist_new_string("QueryType"));
	sendPlist(dict, false);
	plist_free(dict);

	if (!receivePlist(&dict, "QueryType"))
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
			if (lockdownGetValue(NULL, "ProductVersion", &value))
				handleVersionValue(value);
		}
		free(typestr);
	} else {
		qDebug("hmm. QueryType response does not contain a type?!");
		resetDevice(u8"QueryType响应中未包含Type字段。");
	}

	plist_free(dict);
}

//m_connTxAck 这个值和原版的投屏进程还不太一样，设置的时机不一样。这里我设置的早了。明天得看下这个值设置的早了会不会有什么问题
void MirrorManager::onDeviceConnectionInput(unsigned char *payload, uint32_t payload_length)
{
	//if (m_currentConnectionState == QueryType
	//	|| m_currentConnectionState == ProductionVersion
	//	|| m_currentConnectionState == SetUntrustedHostBUID) {
	//	m_usbDataCache.append((const char *)payload, payload_length);
	//	if (m_usbDataCache.size() <= 4)
	//		return;

	//	uint32_t reqSize = 0;
	//	memcpy(&reqSize, m_usbDataCache.data(), sizeof(reqSize));
	//	reqSize = be32toh(reqSize);
	//	if (m_usbDataCache.size() < reqSize + sizeof(reqSize))
	//		return;

	//	plist_t dict = plist_new_dict();
	//	bool success = receivePlist((unsigned char *)m_usbDataCache.data(), m_usbDataCache.size(), &dict);
	//	m_usbDataCache.clear();
	//	if (!success) {
	//		QString msg;
	//		switch (m_currentConnectionState)
	//		{
	//		case MirrorManager::QueryType:
	//			msg = u8"QueryType响应数据解析异常。";
	//			break;
	//		case MirrorManager::ProductionVersion:
	//			msg = u8"ProductionVersion响应数据解析异常。";
	//			break;
	//		default:
	//			break;
	//		}
	//		resetDevice(msg);
	//		plist_free(dict);
	//		return;
	//	}

	//	if (m_currentConnectionState == QueryType) {
	//		plist_t type_node = plist_dict_get_item(dict, "Type");
	//		if (type_node && (plist_get_node_type(type_node) == PLIST_STRING)) {
	//			char* typestr = NULL;
	//			plist_get_string_val(type_node, &typestr);
	//			qDebug("success with type %s", typestr);

	//			if (strcmp(typestr, "com.apple.mobile.lockdown") != 0) {
	//				int cc = 0;
	//				cc++; 
	//			} else {
	//				char *host_id = NULL;
	//				if (config_has_device_record(m_serial)) {
	//					config_device_record_get_host_id(m_serial, &host_id);
	//					//lerr = lockdownd_start_session(lockdown, host_id, NULL, NULL);
	//					//if (host_id)
	//					//	free(host_id);
	//					//if (lerr == LOCKDOWN_E_SUCCESS) {
	//					//	usbmuxd_log(LL_INFO, "%s: StartSession success for device %s", __func__, _dev->udid);
	//					//	usbmuxd_log(LL_INFO, "%s: Finished preflight on device %s", __func__, _dev->udid);
	//					//	client_device_add(info);
	//					//	goto leave;
	//					//}

	//					//usbmuxd_log(LL_INFO, "%s: StartSession failed on device %s, lockdown error %d", __func__, _dev->udid, lerr);
	//				} else {
	//							
	//				}
	//				free(host_id);
	//				lockdownGetValue(NULL, "ProductVersion", ProductionVersion);
	//			}
	//			free(typestr);
	//		} else {
	//			qDebug("hmm. QueryType response does not contain a type?!");
	//			resetDevice(u8"QueryType响应中未包含Type字段。");
	//		}
	//	} else if (m_currentConnectionState == ProductionVersion) {
	//		if (lockdownCheckResult(dict, "GetValue")) {
	//			qDebug() << "success get production version";
	//			plist_t value_node = plist_dict_get_item(dict, "Value");
	//			if (value_node && plist_get_node_type(value_node) == PLIST_STRING) {
	//				char *versionStr = NULL;
	//				plist_get_string_val(value_node, &versionStr);
	//				int versionMajor = strtol(versionStr, NULL, 10);
	//				if (versionMajor < 7)
	//					resetDevice(u8"iOS版本太低，投屏所需最低版本为iOS7。");
	//				else {
	//					qDebug("%s: Found ProductVersion %s device %s", __func__, versionStr, m_serial);

	//					char* system_buid = NULL;
	//					config_get_system_buid(&system_buid);
	//					qDebug("%s: Setting UntrustedHostBUID to %s", __func__, system_buid);
	//					lockdownSetValue(NULL, "UntrustedHostBUID", plist_new_string(system_buid), SetUntrustedHostBUID);
	//					free(system_buid);
	//				}

	//				free(versionStr);
	//			} else
	//				resetDevice(u8"ProductionVersion 获取version字符串失败。");
	//		} else
	//			resetDevice(u8"ProductionVersion GetValue check不通过。");
	//	} else if (m_currentConnectionState == SetUntrustedHostBUID) {
	//		if (lockdownCheckResult(dict, "SetValue")) {
	//			if (!m_devicePaired) {
	//				plist_t options = plist_new_dict();
	//				plist_dict_set_item(options, "ExtendedPairingErrors", plist_new_bool(1));
	//				//lockdownd_error_t ret = lockdownd_do_pair(client, pair_record, "Pair", options, NULL);
	//				plist_free(options);
	//			}
	//		} else
	//			resetDevice(u8"SetUntrustedHostBUID SetValue check不通过。");
	//	}
	//	plist_free(dict);
	//}
}

void MirrorManager::onDeviceTcpInput(struct tcphdr *th, unsigned char *payload, uint32_t payload_length)
{
	uint16_t sport = ntohs(th->th_dport);
	uint16_t dport = ntohs(th->th_sport);

	if(th->th_flags & TH_RST) {
		char *buf = (char *)malloc(payload_length+1);
		memcpy(buf, payload, payload_length);
		if(payload_length && (buf[payload_length-1] == '\n'))
			buf[payload_length-1] = 0;
		buf[payload_length] = 0;
		qDebug("RST reason: %s", buf);
		free(buf);
	}

	if(m_connState == MuxConnState::CONN_CONNECTING) {
		if(th->th_flags != (TH_SYN|TH_ACK)) {
			if(th->th_flags & TH_RST)
				m_connState = MuxConnState::CONN_REFUSED;
			qDebug("Connection refused (%d->%d)", sport, dport);
			resetDevice(u8"设备拒绝连接。"); //this also sends the notification to the client
		} else {
			m_connTxSeq++;
			m_connTxAck++;
			if(sendTcp(TH_ACK, NULL, 0) < 0) {
				qDebug("Error sending TCP ACK to device (%d->%d)", sport, dport);
				resetDevice(u8"发送响应指令失败，请重新连接。");
				return;
			}
			m_connState = MuxConnState::CONN_CONNECTED;

			QMetaObject::invokeMethod(this, "startActualPair");
		}
	} else if(m_connState == MuxConnState::CONN_CONNECTED) {
		m_connTxAck += payload_length;
		if(th->th_flags != TH_ACK) {
			qDebug("Connection reset by device %d (%d->%d)", sport, dport);
			if(th->th_flags & TH_RST)
				m_connState = MuxConnState::CONN_DYING;
			resetDevice(u8"设备重置了连接，请断开设备、重启后再试。");
		} else {
			m_usbDataLock.lock();
			circlebuf_push_back(&m_usbDataCache, payload, payload_length);
			m_usbDataWaitCondition.wakeOne();
			m_usbDataLock.unlock();

			// Device likes it best when we are prompty ACKing data
			sendTcpAck();
		}
	}
}

bool MirrorManager::readDataWithSize(void *dst, size_t size, int timeout)
{
	bool ret = false;
	QMutexLocker locker(&m_usbDataLock);
	while (m_usbDataCache.size < size) {
		ret = m_usbDataWaitCondition.wait(&m_usbDataLock, timeout);
		if (!ret)
			break;
	}

	if (m_usbDataCache.size >= size) {
		circlebuf_pop_front(&m_usbDataCache, dst, size);
		ret = true;
	} else {
		ret = false;
		resetDevice(u8"读取usb数据超时。");
	}

	return ret;
}

void MirrorManager::onDeviceData(unsigned char *buffer, int length)
{
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
			buffer = m_pktbuf;
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

	MuxHeader *mhdr = (MuxHeader *)buffer;
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

bool MirrorManager::sendPlist(plist_t plist, bool isBinary)
{
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

	sendTcp(TH_ACK, (const unsigned char *)sendBuffer, total);

	free(content);
	free(sendBuffer);

	m_connTxSeq += total;

	return true;
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
	int ret = -1;
	struct usb_device *device = findAppleDevice(vid, pid);
	if (!device) {
		m_errorMsg = u8"iOS设备未找到";
		return false;
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
					m_qtConfigIndex = qtConfigIndex;
					m_deviceHandle = usb_open(dev);
					m_device = dev;
					memset(m_serial, 0, sizeof(m_serial));
					usb_get_string_simple(m_deviceHandle, m_device->descriptor.iSerialNumber, m_serial, sizeof(m_serial));
					ret = true;
				}
				break;
			}
		}
		qDebug() << "Apple device reconnect cost time " << t.elapsed() << ", loop count: " << loopCount;
	} else
		m_errorMsg = u8"设备异常，请重新插拔手机后再试。";

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
	if (config != m_qtConfigIndex)
		usb_set_configuration(m_deviceHandle, m_qtConfigIndex);

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
	return true;
}

bool MirrorManager::startPair()
{
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

	m_pairBlockEvent.exec();

	return true;
}

//-1=>没找到设备
//-2=>发送切换指令后，设备触发了重启，但是重启后还是没有启用新USB功能，提示用户重启手机
//-3=>设备已经在启用了投屏能力，提示用户插拔手机
//-4=>发送切换指令后，3s内未能重新找到设备，提示用户保持手机与电脑的连接
bool MirrorManager::startMirrorTask(int vid, int pid) 
{
	m_pktlen = 0;
	return checkAndChangeMode(vid, pid) && setupUSBInfo() && startPair();
}

int MirrorManager::sendTcpAck()
{
	if (sendTcp(TH_ACK, NULL, 0) < 0) {
		qDebug("Error sending TCP ACK");
		resetDevice(u8"发送响应指令失败，请重新连接。");
		return -1;
	}

	return 0;
}

int MirrorManager::sendTcp(uint8_t flags, const unsigned char *data, int length)
{
	struct tcphdr th;
	memset(&th, 0, sizeof(th));
	th.th_sport = htons(233);
	th.th_dport = htons(0xf27e);
	th.th_seq = htonl(m_connTxSeq);
	th.th_ack = htonl(m_connTxAck);
	th.th_flags = flags;
	th.th_off = sizeof(th) / 4;
	th.th_win = htons(131072 >> 8);

	return sendPacket(MuxProtocol::MUX_PROTO_TCP, &th, data, length);
}

int MirrorManager::sendPacket(MuxProtocol proto, void *header, const void *data, int length)
{
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

	m_usbSendLock.lock();
	res = usb_bulk_write(m_deviceHandle, ep_out, buffer, total, 1000);
	m_usbSendLock.unlock();
	free(buffer);
	if(res < 0) {
		qDebug("usb_send failed while sending packet (len %d) to device", total);
		return res;
	}
	return total;
}
