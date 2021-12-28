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
#include <QThread>
#include "DriverHelper.h"
#include <windows.h>
#include <dbt.h>


extern GUID ADB_DEVICE_GUID;
extern GUID USB_DEVICE_GUID;

struct APPLE_DEVICE_INFO {
	int vid;
	int pid;
	QString devicePath;
};

class ReadStdinThread : public QThread {
	Q_OBJECT
public:
	ReadStdinThread(QObject *parent = nullptr);

protected:
	virtual void run() override;

signals:
	void quit();
};

class MirrorManager;
class AppleDeviceManager : public QObject {
	Q_OBJECT
public:
	AppleDeviceManager();
	~AppleDeviceManager();

	bool enumDeviceAndCheck();
	int checkAndInstallDriver();
	void deferUpdateUsbInventory(bool isAdd);
	void startTask();

	void signalWait()
	{
		m_waitMutex.lock();
		m_waitCondition.wakeOne();
		m_waitMutex.unlock();
	}

signals:
	void installProgress(int value);
	void installError(QString msg);
	void infoPrompt(const QString &msg);
	void deviceLost();

public slots:
	void updateUsbInventory(bool isDeviceAdd);

private:
	QMutex m_deviceChangeMutex;
	QMutex m_waitMutex;
	QWaitCondition m_waitCondition;
	DriverHelper *m_driverHelper = nullptr;
	APPLE_DEVICE_INFO m_appleDeviceInfo = { 0 };
	bool m_inScreenMirror = false;
	MirrorManager *m_mirrorManager = nullptr;
	ReadStdinThread *m_readStdinThread = nullptr;
};

class HelerWidget : public QWidget {
	Q_OBJECT
public:
	HelerWidget(AppleDeviceManager *helper, QWidget *parent = nullptr)
		: QWidget(parent)
	{
		connect(this, &HelerWidget::updateDevice, helper, &AppleDeviceManager::deferUpdateUsbInventory, Qt::DirectConnection);

		auto registerNotification = [this](GUID id, HDEVNOTIFY &ret) {
			DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
			ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
			NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
			NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
			NotificationFilter.dbcc_classguid = id;

			ret = RegisterDeviceNotification(
				(HWND)winId(),              // events recipient
				&NotificationFilter,        // type of device
				DEVICE_NOTIFY_WINDOW_HANDLE // type of recipient handle
			);
		};

		registerNotification(USB_DEVICE_GUID, hDeviceNotify_normal);
	}

	~HelerWidget()
	{
		UnregisterDeviceNotification(hDeviceNotify_normal);
	}

protected:
	virtual bool nativeEvent(const QByteArray &eventType, void *message,
				 long *result)
	{
		MSG *winMsg = static_cast<MSG *>(message);
		if (winMsg->message == WM_DEVICECHANGE) {
			switch (winMsg->wParam) {
			case DBT_DEVICEARRIVAL: {
				auto info = (DEV_BROADCAST_DEVICEINTERFACE *)winMsg->lParam;

				if (info->dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
					QString devicePath = QString::fromWCharArray(info->dbcc_name);
					qDebug() << "new device: " << devicePath;

					emit updateDevice(true);
				}
			} break;
			case DBT_DEVICEREMOVECOMPLETE:
			{
				auto info = (DEV_BROADCAST_DEVICEINTERFACE *)winMsg->lParam;
				if (info->dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
					QString devicePath = QString::fromWCharArray(info->dbcc_name);
					qDebug() << "device remove: " << devicePath;
				}
				emit updateDevice(false);
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

signals:
	void updateDevice(bool isAdd);

private:
	HDEVNOTIFY hDeviceNotify_normal;
};

#endif // AOADEVICEMANAGER_H
