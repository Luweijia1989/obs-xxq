#pragma once

#include <stdint.h>
#include <thread>
#include <QEventLoop>
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <util/circlebuf.h>
#include "plist/plist.h"

extern "C"
{
#include "qt_configuration.h"
}

#define DEV_MRU 65536
#define USB_MTU (3 * 16384)
#define USB_MRU 16384

enum class MuxProtocol {
	MUX_PROTO_VERSION = 0,
	MUX_PROTO_CONTROL = 1,
	MUX_PROTO_SETUP = 2,
	MUX_PROTO_TCP = IPPROTO_TCP
};

struct VersionHeader {
	uint32_t major;
	uint32_t minor;
	uint32_t padding;
};

struct MuxHeader {
	uint32_t protocol;
	uint32_t length;
	uint32_t magic;
	uint16_t tx_seq;
	uint16_t rx_seq;
};

enum class MuxDevState {
	MUXDEV_INIT,   // sent version packet
	MUXDEV_ACTIVE, // received version packet, active
	MUXDEV_DEAD    // dead
};

enum class MuxConnState {
	CONN_CONNECTING, // SYN
	CONN_CONNECTED,  // SYN/SYNACK/ACK -> active
	CONN_REFUSED,    // RST received during SYN
	CONN_DYING,      // RST received
	CONN_DEAD // being freed; used to prevent infinite recursion between client<->device freeing
};


class MirrorManager : public QObject
{
	Q_OBJECT
public:

	struct PairRecord
	{
		char *device_certificate;
		char *host_certificate;
		char *root_certificate;
		char *host_id;
		char *system_buid;
	};

	MirrorManager();
	~MirrorManager();

	bool startMirrorTask(int vid, int pid);

	static void readUSBData(void *data);

private:
	bool checkAndChangeMode(int vid, int pid);
	bool setupUSBInfo();
	bool startPair();

private:
	int sendTcpAck();
	int sendTcp(uint8_t flags, const unsigned char *data, int length);
	int sendPacket(MuxProtocol proto, void *header, const void *data, int length);
	bool sendPlist(plist_t plist, bool isBinary);
	bool receivePlistInternal(char *payload, uint32_t payload_length, plist_t *plist);
	void onDeviceData(unsigned char *buffer, int length);
	void onDeviceVersionInput(VersionHeader *vh);
	void onDeviceTcpInput(struct tcphdr *th, unsigned char *payload, uint32_t payload_length);
	void onDeviceConnectionInput(unsigned char *payload, uint32_t payload_length);
	void resetDevice(QString msg);
	bool readDataWithSize(void *dst, size_t size, int timeout = 100);

	void setUntrustedHostBuid();
	bool receivePlist(plist_t *ret, const char *key);
	void handleVersionValue(plist_t value);

	bool lockdownPair(PairRecord *record);
	bool lockdownDoPair(PairRecord *pair_record, const char *verb, plist_t options, plist_t *result);
	bool lockdownPairRecordgenerate(plist_t *pair_record);
	bool lockdownGetDevicePublicKeyAsKeyData(char **data, uint64_t *size);
public slots:
	void startActualPair();
	bool lockdownGetValue(const char *domain, const char *key, plist_t *value);
	bool lockdownSetValue(const char *domain, const char *key, plist_t value);
	//bool lockdownd_do_pair(PairRecord *pair_record, const char *verb, plist_t options, plist_t *result);

private:
	QString m_errorMsg;
	unsigned char *m_pktbuf = nullptr;
	uint32_t m_pktlen;

	bool m_stop = false;
	char m_serial[256] = { 0 };
	int m_qtConfigIndex = -1;
	uint8_t m_interface, ep_in, ep_out;
	uint8_t m_interface_fa, ep_in_fa, ep_out_fa;
	struct usb_dev_handle *m_deviceHandle = nullptr;
	struct usb_device *m_device = nullptr;
	QMutex m_usbSendLock;
	std::thread m_usbReadTh;

	bool m_pairFinished = false;
	bool m_devicePaired = false;

	circlebuf m_usbDataCache;
	QMutex m_usbDataLock;
	QWaitCondition m_usbDataWaitCondition;

	uint32_t m_reqReadSize;
	uint16_t m_rxSeq;
	uint16_t m_txSeq;
	u_long m_connTxSeq;
	u_long m_connTxAck;
	int m_devVersion;
	MuxDevState m_devState;
	MuxConnState m_connState;
	QEventLoop m_pairBlockEvent;
	QEventLoop m_pairStepBlockEvent;
};
