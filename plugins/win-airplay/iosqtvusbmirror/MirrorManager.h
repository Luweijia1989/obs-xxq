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
#include "conf.h"
#include "userpref.h"
#include "utils.h"
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

	struct PairRecord {
		char *device_certificate;
		char *host_certificate;
		char *root_certificate;
		char *host_id;
		char *system_buid;
	};

	struct LockdownServiceDescriptor {
		uint16_t port;
		uint8_t ssl_enabled;
	};

	enum ConnectionType {
		InitPair,
		PairNotify,
		StartService,
		Other,
	};

	struct ConnectionInfo {
		u_long connTxSeq;
		u_long connTxAck;
		uint16_t srcPort;
		uint16_t dstPort;
		ConnectionType type;
		MuxConnState connState;

		circlebuf m_usbDataCache;

		ConnectionInfo(ConnectionType t, uint16_t sport, uint16_t dport) {
			type = t;
			connTxAck = 0;
			connTxSeq = 0;
			connState = MuxConnState::CONN_CONNECTING;
			srcPort = sport;
			dstPort = dport;
			circlebuf_init(&m_usbDataCache);
		}

		~ConnectionInfo() {
			circlebuf_free(&m_usbDataCache);
		}
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
	void startConnect(ConnectionType type, uint16_t dport);
	int sendTcpAck(ConnectionInfo *conn);
	int sendTcp(ConnectionInfo *conn, uint8_t flags, const unsigned char *data, int length);
	int sendPacket(MuxProtocol proto, void *header, const void *data, int length);
	bool sendPlist(ConnectionInfo *conn, plist_t plist, bool isBinary);
	int sendAnonRst(uint16_t sport, uint16_t dport, uint32_t ack);
	bool receivePlistInternal(char *payload, uint32_t payload_length, plist_t *plist);
	void onDeviceVersionInput(VersionHeader *vh);
	void onDeviceTcpInput(struct tcphdr *th, unsigned char *payload, uint32_t payload_length);
	bool readDataWithSize(ConnectionInfo *conn, void *dst, size_t size, bool allowTimeout, int timeout = 500);

	void startActualPair(void *conn);
	void startObserve(void *conn);
	void startFinalPair(ConnectionInfo *conn);
	void pairResult(bool success);

	void setUntrustedHostBuid(ConnectionInfo *conn);
	bool receivePlist(ConnectionInfo *conn, plist_t *ret, const char *key, bool allowTimeout = false);
	void handleVersionValue(ConnectionInfo *conn, plist_t value);
	
	bool lockdownGetValue(ConnectionInfo *conn, const char *domain, const char *key, plist_t *value);
	bool lockdownSetValue(ConnectionInfo *conn, const char *domain, const char *key, plist_t value);
	bool lockdownPair(ConnectionInfo *conn, PairRecord *record, bool raiseError = false);
	bool lockdownDoPair(ConnectionInfo *conn, PairRecord *pair_record, const char *verb, plist_t options, plist_t *result, bool raiseError);
	bool lockdownPairRecordgenerate(ConnectionInfo *conn, plist_t *pair_record);
	bool lockdownGetDevicePublicKeyAsKeyData(ConnectionInfo *conn, key_data_t *public_key);

	bool lockdownStartService(ConnectionInfo *conn, const char *identifier, LockdownServiceDescriptor **service);
	bool lockdownDoStartService(ConnectionInfo *conn, const char *identifier, int send_escrow_bag, LockdownServiceDescriptor **service);
	bool lockdownBuildStartServiceRequest(const char *identifier, int send_escrow_bag, plist_t *request);
	void lockdownObserveNotification(const char *notification);
	void lockdownProcessNotification(const char *notification);
public slots:
	void onDeviceData(QByteArray data);
	void resetDevice(QString msg, bool closeDevice = true);

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
	std::thread m_usbReadTh;

	bool m_devicePaired = false;

	QTimer *m_notificationTimer = nullptr;

	QList<ConnectionInfo *> m_connections;
	uint16_t m_initPort = 1;
	uint16_t m_rxSeq;
	uint16_t m_txSeq;
	int m_devVersion;
	MuxDevState m_devState;
	QEventLoop *m_pairBlockEvent;
	QEventLoop *m_usbDataBlockEvent;
};
