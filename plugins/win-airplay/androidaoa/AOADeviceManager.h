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

extern GUID ADB_DEVICE_GUID;
extern GUID USB_DEVICE_GUID;

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
} accessory_droid;

struct ANDROID_DEVICE_INFO
{
	int vid;
	int pid;
	QString devicePath;
};

class AOADeviceManager : public QObject {
	Q_OBJECT
public:
	AOADeviceManager();
	~AOADeviceManager();

	int connectDevice(libusb_device *device);
	int isDroidInAcc(int vid, int pid);
	void switchDroidToAcc(int vid, int pid, int force);
	int setupDroid(libusb_device *usbDevice, accessory_droid *device);
	int shutdownUSBDroid(accessory_droid *device);
	int startUSBPipe();
	void stopUSBPipe();

	bool enumDeviceAndCheck();
	int checkAndInstallDriver();
	bool startTask();
	void deferUpdateUsbInventory(bool isAdd);
	bool adbDeviceOpenAndCheck(WCHAR *devicePath);

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
	bool isAndroidADBDevice(int vid, int pid);
	bool handleMediaData();
	bool adbDeviceExist();

signals:
	void installProgress(int step, int value);
	void installError(QString msg);
	void infoPrompt(const QString &msg);
	void deviceLost();

public slots:
	void updateUsbInventory(bool isDeviceAdd, bool checkAdb);
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
	ANDROID_DEVICE_INFO m_androidDeviceInfo = { 0 };

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
	HelerWidget(AOADeviceManager *helper, QWidget *parent = nullptr)
		: m_helper(helper), QWidget(parent)
	{
		auto registerNotification = [this](GUID id, HDEVNOTIFY &ret) {
			DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
			ZeroMemory(&NotificationFilter,
				   sizeof(NotificationFilter));
			NotificationFilter.dbcc_size =
				sizeof(DEV_BROADCAST_DEVICEINTERFACE);
			NotificationFilter.dbcc_devicetype =
				DBT_DEVTYP_DEVICEINTERFACE;
			NotificationFilter.dbcc_classguid = id;

			ret = RegisterDeviceNotification(
				(HWND)winId(),              // events recipient
				&NotificationFilter,        // type of device
				DEVICE_NOTIFY_WINDOW_HANDLE // type of recipient handle
			);
		};

		registerNotification(USB_DEVICE_GUID, hDeviceNotify_normal);
		registerNotification(ADB_DEVICE_GUID, hDeviceNotify_adb);

		timer.setSingleShot(true);
		timer.setInterval(2000);
		connect(&timer, &QTimer::timeout, this,
			[=]() { m_helper->deferUpdateUsbInventory(true); });
	}

	~HelerWidget()
	{
		UnregisterDeviceNotification(hDeviceNotify_normal);
		UnregisterDeviceNotification(hDeviceNotify_adb);
	}

protected:
	virtual bool nativeEvent(const QByteArray &eventType, void *message,
				 long *result)
	{
		MSG *winMsg = static_cast<MSG *>(message);
		if (winMsg->message == WM_DEVICECHANGE) {
			switch (winMsg->wParam) {
			case DBT_DEVICEARRIVAL: {
				auto info = (DEV_BROADCAST_DEVICEINTERFACE *)
						    winMsg->lParam;

				if (info->dbcc_devicetype ==
				    DBT_DEVTYP_DEVICEINTERFACE) {
					QString devicePath =
						QString::fromWCharArray(
							info->dbcc_name);
					qDebug() << "new device: " << devicePath;
					bool isAdbDevice = devicePath.endsWith(
						"{f72fe0d4-cbcb-407d-8814-9ed673d0dd6b}", Qt::CaseInsensitive);
					if (timer.isActive())
						timer.stop();

					if (!isAdbDevice) {
						timer.start();
						m_helper->signalWait();
					} else {
						if (!m_helper->adbDeviceOpenAndCheck(
							    info->dbcc_name))
							m_helper->deferUpdateUsbInventory(
								true);
						else
							emit m_helper->infoPrompt(
								u8"系统检测到手机的'USB调试'功能被打开，请在手机'设置'界面中'开发人员选项'里关闭此功能，并重新拔插手机！！");
					}
				}
			} break;
			case DBT_DEVICEREMOVECOMPLETE:
			{
				auto info = (DEV_BROADCAST_DEVICEINTERFACE *)
						    winMsg->lParam;
				if (info->dbcc_devicetype ==
				    DBT_DEVTYP_DEVICEINTERFACE) {
					QString devicePath =
						QString::fromWCharArray(
							info->dbcc_name);
					qDebug() << "device remove: " << devicePath;
				}
				m_helper->deferUpdateUsbInventory(false);
			}
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
	HDEVNOTIFY hDeviceNotify_normal;
	HDEVNOTIFY hDeviceNotify_adb;
	AOADeviceManager *m_helper;
	QTimer timer;
};

#endif // AOADEVICEMANAGER_H
