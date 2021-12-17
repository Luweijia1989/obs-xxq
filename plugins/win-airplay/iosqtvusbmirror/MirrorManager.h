#pragma once

#include <stdint.h>
#include <thread>
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
	MUX_PROTO_SETUP = 2
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

class MirrorManager
{
public:
	MirrorManager();
	~MirrorManager();

	int startMirrorTask(int vid, int pid);

	static void readUSBData(void *data);

private:
	int checkAndChangeMode(int vid, int pid);
	int setupUSBInfo();
	int startPair();

private:
	int sendPacket(MuxProtocol proto, void *header, const void *data, int length);
	void onDeviceData(unsigned char *buffer, int length);

private:
	unsigned char *m_pktbuf = nullptr;
	uint32_t m_pktlen;

	bool m_stop = false;
	char serial[256] = { 0 };
	int m_qtConfigIndex = -1;
	uint8_t m_interface, ep_in, ep_out;
	uint8_t m_interface_fa, ep_in_fa, ep_out_fa;
	struct usb_dev_handle *m_deviceHandle = nullptr;
	struct usb_device *m_device = nullptr;
	std::thread m_usbReadTh;
};
