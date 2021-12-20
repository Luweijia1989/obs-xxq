#include "MirrorManager.h"
#include "tcp.h"
#include "endianness.h"
#include <QDebug>
#include <QElapsedTimer>

MirrorManager::MirrorManager()
{
	usb_init();
	m_pktbuf = (unsigned char *)malloc(DEV_MRU);
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
			if (readLen != -116)
				break;
		} else
			manager->onDeviceData((unsigned char *)buffer, readLen);
	}
	free(buffer);
}

void MirrorManager::resetDevice()
{

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

void MirrorManager::onDeviceConnectionInput(unsigned char *payload, uint32_t payload_length)
{
	if (m_currentConnectionState == QueryType) {

	}
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
			resetDevice(); //this also sends the notification to the client
		} else {
			m_connTxSeq++;
			m_connTxAck++;
			if(sendTcp(TH_ACK, NULL, 0) < 0) {
				qDebug("Error sending TCP ACK to device (%d->%d)", sport, dport);
				resetDevice();
				return;
			}
			m_connState = MuxConnState::CONN_CONNECTED;

			startActualPair();
		}
	} else if(m_connState == MuxConnState::CONN_CONNECTED) {
		m_connTxAck += payload_length;
		if(th->th_flags != TH_ACK) {
			qDebug("Connection reset by device %d (%d->%d)", sport, dport);
			if(th->th_flags & TH_RST)
				m_connState = MuxConnState::CONN_DYING;
			resetDevice();
		} else {
			onDeviceConnectionInput(payload, payload_length);

			// Device likes it best when we are prompty ACKing data
			sendTcpAck();
		}
	}
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
		if (length <
		    (mux_header_size + sizeof(VersionHeader))) {
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

static void plist_dict_add_label(plist_t plist, const char *label)
{
	if (plist && label) {
		if (plist_get_node_type(plist) == PLIST_DICT)
			plist_dict_set_item(plist, "Label", plist_new_string(label));
	}
}

void MirrorManager::startActualPair()
{
	m_currentConnectionState = QueryType;
	plist_t dict = plist_new_dict();
	plist_dict_add_label(dict, "usbmuxd");
	plist_dict_set_item(dict, "Request", plist_new_string("QueryType"));
	sendPlist(dict, false);
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
int MirrorManager::checkAndChangeMode(int vid, int pid)
{
	int ret = -1;
	struct usb_device *device = findAppleDevice(vid, pid);
	if (!device)
		return ret;

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
					ret = -4;
					break;
				}
				continue;
			} else {
				findConfigurations(&dev->descriptor, dev, &muxConfigIndex, &qtConfigIndex);
				if (qtConfigIndex == -1)
					ret = -2;
				else {
					m_qtConfigIndex = qtConfigIndex;
					m_deviceHandle = usb_open(dev);
					m_device = dev;
					memset(m_serial, 0, sizeof(m_serial));
					usb_get_string_simple(m_deviceHandle, m_device->descriptor.iSerialNumber, m_serial, sizeof(m_serial));
					ret = 0;
				}
				break;
			}
		}
		qDebug() << "Apple device reconnect cost time " << t.elapsed() << ", loop count: " << loopCount;
	} else
		ret = -3;

	return ret;
}

int MirrorManager::setupUSBInfo()
{
	if(!m_device || !m_deviceHandle)
		return -1;

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
	return 0;
}

int MirrorManager::startPair()
{
	m_usbReadTh = std::thread(readUSBData, this);
	m_usbReadTh.detach();

	m_currentConnectionState = None;
	m_devVersion = 0;
	m_devState = MuxDevState::MUXDEV_INIT;
	VersionHeader vh;
	vh.major = htonl(2);
	vh.minor = htonl(0);
	vh.padding = 0;
	sendPacket(MuxProtocol::MUX_PROTO_VERSION, &vh, NULL, 0);

	m_pairBlockEvent.exec();

	return 0;
}

//-1=>没找到设备
//-2=>发送切换指令后，设备触发了重启，但是重启后还是没有启用新USB功能，提示用户重启手机
//-3=>设备已经在启用了投屏能力，提示用户插拔手机
//-4=>发送切换指令后，3s内未能重新找到设备，提示用户保持手机与电脑的连接
int MirrorManager::startMirrorTask(int vid, int pid) 
{
	m_pktlen = 0;
	int ret = -1;
	do {
		ret = checkAndChangeMode(vid, pid);
		if (ret != 0)
			break;

		ret = setupUSBInfo();
		if (ret != 0)
			break;

		ret = startPair();
		if (ret != 0)
			break;
	} while (false);

	return ret;
}

int MirrorManager::sendTcpAck()
{
	if (sendTcp(TH_ACK, NULL, 0) < 0) {
		qDebug("Error sending TCP ACK");
		resetDevice();
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

	res = usb_bulk_write(m_deviceHandle, ep_out, buffer, total, 1000);
	free(buffer);
	if(res < 0) {
		qDebug("usb_send failed while sending packet (len %d) to device", total);
		return res;
	}
	return total;
}
