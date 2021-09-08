#ifndef AOADEVICEMANAGER_H
#define AOADEVICEMANAGER_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QTimer>
#include <QFile>
#include <QSet>
#include "libusb.h"
#include "DriverHelper.h"
#include "util/circlebuf.h"

#define USE_AUDIO

#define AOA_PROTOCOL_MIN 1
#define AOA_PROTOCOL_MAX 2

#define VID_GOOGLE 0x18D1
#define PID_AOA_ACC 0x2D00
#define PID_AOA_ACC_ADB 0x2D01
#define PID_AOA_AU 0x2D02
#define PID_AOA_AU_ADB 0x2D03
#define PID_AOA_ACC_AU 0x2D04
#define PID_AOA_ACC_AU_ADB 0x2D05

typedef void (*fnusb_iso_cb)(uint8_t *buf, int len);

typedef struct {
	struct libusb_transfer **xfers;
	uint8_t *buffer;
	fnusb_iso_cb cb;
	int num_xfers;
	int pkts;
	int len;
	int dead;
	int dead_xfers;
} fnusb_isoc_stream;

typedef struct t_accessory_droid {
	libusb_device_handle *usbHandle = nullptr;
	unsigned char inendp;
	unsigned char outendp;
	unsigned char audioendp;

	int inpacketsize;
	int outpacketsize;
	int audiopacketsize;

	unsigned char bulkInterface;
	unsigned char audioInterface;
	unsigned char audioAlternateSetting;

	bool connected = false;
	uint16_t c_vid;
	uint16_t c_pid;

	fnusb_isoc_stream isocStream;
} accessory_droid;

class AOADeviceManager : public QObject {
	Q_OBJECT
public:
	bool inUpdate = false;
	AOADeviceManager();
	~AOADeviceManager();

	int connectDevice(libusb_device *device);
	int isDroidInAcc(libusb_device *dev);
	void switchDroidToAcc(libusb_device *dev, int force);
	int setupDroid(libusb_device *usbDevice, accessory_droid *device);
	int shutdownUSBDroid(accessory_droid *device);
	int startUSBPipe();
	void stopUSBPipe();

	bool enumDeviceAndCheck();
	bool checkAndInstallDriver();
	bool startTask();

	void signalWait()
	{
		m_waitMutex.lock();
		m_waitCondition.wakeOne();
		m_waitMutex.unlock();
	}

	static void *a2s_usbRxThread(void *d);
	static void *startThread(void *d);

private:
	int initUSB();
	void clearUSB();
	bool isAndroidDevice(libusb_device *device, struct libusb_device_descriptor &desc);
	bool isAndroidADBDevice(struct libusb_device_descriptor &desc);
	bool handleMediaData();

signals:
	void installProgress(int step, int value);
	void infoPrompt(const QString &msg);
	void deviceLost();

public slots:
	void updateUsbInventory();
	void disconnectDevice();

private:
	libusb_context *m_ctx = nullptr;
	accessory_droid m_droid;
	libusb_device **m_devs = nullptr;
	int m_devs_count = 0;
	std::thread m_usbReadThread;
	std::thread m_startThread;
	bool m_continuousRead = false;
	bool m_exitStart = false;
	unsigned char *buffer = nullptr;
	DriverHelper *m_driverHelper = nullptr;

	QMutex m_waitMutex;
	QWaitCondition m_waitCondition;

	circlebuf m_mediaDataBuffer;
	struct IPCClient *client = NULL;
	uint8_t *m_cacheBuffer = nullptr;

	IPCClient *m_client = nullptr;
	QSet<int> m_vids;
	QFile h264;
};

class NativeEventFilter : public QAbstractNativeEventFilter {
public:
	NativeEventFilter(AOADeviceManager *helper) : m_helper(helper) {}
	bool nativeEventFilter(const QByteArray &eventType, void *message,
			       long *) override
	{
		Q_UNUSED(eventType)
		MSG *msg = static_cast<MSG *>(message);
		int msgType = msg->message;
		if (msgType == WM_DEVICECHANGE) {
			if (!m_helper->inUpdate)
				QMetaObject::invokeMethod(m_helper,
							  "updateUsbInventory",
							  Qt::QueuedConnection);
			else {
				QTimer::singleShot(200, m_helper, [=]() {
					m_helper->signalWait();
				});
			}
		}
		return false;
	}

private:
	AOADeviceManager *m_helper;
};

#endif // AOADEVICEMANAGER_H
