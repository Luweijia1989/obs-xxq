#ifndef AOADEVICEMANAGER_H
#define AOADEVICEMANAGER_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QTimer>
#include <QFile>
#include <QDebug>
#include <QSet>
#include <QWidget>
#include "libusb.h"
#include "DriverHelper.h"
#include "util/circlebuf.h"
#include <windows.h>
#include <dbt.h>

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
	int checkAndInstallDriver();
	bool startTask();
	void deferUpdateUsbInventory(bool isAdd);

	void signalWait()
	{
		m_waitMutex.lock();
		m_waitCondition.wakeOne();
		m_waitMutex.unlock();
	}

	static void *a2s_usbRxThread(void *d);
	static void *startThread(void *d);
	static void *heartbeatThread(void *d);

private:
	int initUSB();
	void clearUSB();
	int isAndroidDevice(libusb_device *device,
			    struct libusb_device_descriptor &desc);
	bool isAndroidADBDevice(struct libusb_device_descriptor &desc);
	bool handleMediaData();
	bool adbDeviceExist();

signals:
	void installProgress(int step, int value);
	void infoPrompt(const QString &msg);
	void deviceLost();

public slots:
	void updateUsbInventory(bool isDeviceAdd, bool needSleepForAdbCheck);
	void disconnectDevice();

private:
	QMutex m_deviceChangeMutex;
	libusb_context *m_ctx = nullptr;
	accessory_droid m_droid;
	libusb_device **m_devs = nullptr;
	int m_devs_count = 0;
	std::thread m_usbReadThread;
	std::thread m_startThread;
	std::thread m_heartbeatThread;
	QMutex m_usbMutex;
	QWaitCondition m_timeoutCondition;
	QMutex m_timeoutMutex;
	bool m_continuousRead = false;
	bool m_exitStart = false;
	unsigned char *buffer = nullptr;
	DriverHelper *m_driverHelper = nullptr;

	QMutex m_waitMutex;
	QWaitCondition m_waitCondition;

	circlebuf m_mediaDataBuffer;
	struct IPCClient *client = NULL;
	uint8_t *m_cacheBuffer = nullptr;
	size_t m_cacheBufferSize = 0;

	IPCClient *m_client = nullptr;
	QSet<int> m_vids;
	QFile h264;
};

class HelerWidget : public QWidget {
	Q_OBJECT
public:
	HelerWidget(AOADeviceManager *helper, QWidget *parent = nullptr) : m_helper(helper), QWidget(parent) {
		GUID WceusbshGUID = {0xA5DCBF10, 0x6530, 0x11D2, 0x90,
				     0x1F,       0x00,   0xC0,   0x4F,
				     0xB9,       0x51,   0xED};

		DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
		ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
		NotificationFilter.dbcc_size =
			sizeof(DEV_BROADCAST_DEVICEINTERFACE);
		NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		NotificationFilter.dbcc_classguid = WceusbshGUID;

		hDeviceNotify = RegisterDeviceNotification(
			(HWND)winId(),                       // events recipient
			&NotificationFilter,        // type of device
			DEVICE_NOTIFY_WINDOW_HANDLE // type of recipient handle
		);
	}

	~HelerWidget() {
		UnregisterDeviceNotification(hDeviceNotify);
	}

protected:
	virtual bool nativeEvent(const QByteArray &eventType, void *message, long *result) {
		MSG* winMsg = static_cast<MSG *>(message);
		if (winMsg->message == WM_DEVICECHANGE) {
			switch (winMsg->wParam) {
			case DBT_DEVICEARRIVAL:
				m_helper->deferUpdateUsbInventory(true);
				break;
			case DBT_DEVICEREMOVECOMPLETE:
				m_helper->deferUpdateUsbInventory(false);
				break;
			case DBT_DEVNODES_CHANGED:
				break;
			default:
				break;
			}
		}

		return QWidget::nativeEvent(eventType, message, result);
	}

private:
	HDEVNOTIFY hDeviceNotify;
	AOADeviceManager *m_helper;
};

#endif // AOADEVICEMANAGER_H
