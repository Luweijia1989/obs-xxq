#pragma once

#include <Windows.h>
#include <Dbt.h>
#include <qabstractnativeeventfilter.h>
#include <qobject.h>
#include <qwidget.h>
#include <qtimer.h>
#include <qprocess.h>
#include <qeventloop.h>
#include <qset.h>

#include "usb-helper.h"

class NativeEventFilter : public QObject, public QAbstractNativeEventFilter {
	Q_OBJECT
private:
	void registerNotification(GUID id, HDEVNOTIFY &ret)
	{
		DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
		ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
		NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
		NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		NotificationFilter.dbcc_classguid = id;

		ret = RegisterDeviceNotification((HWND)m_dummyWidget->winId(), // events recipient
						 &NotificationFilter,          // type of device
						 DEVICE_NOTIFY_WINDOW_HANDLE   // type of recipient handle
		);
	}

public:
	NativeEventFilter()
	{
		m_dummyWidget = new QWidget; // need for receive device add and remove event
		registerNotification(USB_DEVICE_GUID, m_deviceNotify);
	}

	~NativeEventFilter()
	{
		UnregisterDeviceNotification(m_deviceNotify);
		delete m_dummyWidget;
	}
	virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *result)
	{
		Q_UNUSED(result)

		if (eventType == "windows_generic_MSG") {
			MSG *winMsg = static_cast<MSG *>(message);
			if (winMsg->message == WM_DEVICECHANGE) {
				switch (winMsg->wParam) {
				case DBT_DEVICEARRIVAL: {
					auto info = (DEV_BROADCAST_DEVICEINTERFACE *)winMsg->lParam;

					if (info->dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
						QString devicePath = QString::fromWCharArray(info->dbcc_name);
						emit deviceChange(devicePath.toLower(), true);
					}
				} break;
				case DBT_DEVICEREMOVECOMPLETE: {
					auto info = (DEV_BROADCAST_DEVICEINTERFACE *)winMsg->lParam;
					if (info->dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
						QString devicePath = QString::fromWCharArray(info->dbcc_name);
						emit deviceChange(devicePath.toLower(), false);
					}
				} break;
				case DBT_DEVNODES_CHANGED:
					break;
				default:
					break;
				}
			}
		}

		return false;
	}

signals:
	void deviceChange(QString devicePath, bool isAdd);

private:
	HDEVNOTIFY m_deviceNotify;
	QWidget *m_dummyWidget;
};

class DriverHelper : public QObject {
	Q_OBJECT
public:
	DriverHelper(QObject *parent = nullptr);
	~DriverHelper();

	void checkDevices();

signals:
	void driverReady();

private:
	void initAndroidVids();
	QList<QString> enumUSBDevice();
	bool doDriverProcess(QString devicePath, bool checkAoA = false);
	void switchDroidToAcc(int vid, int pid, int force);

private:
	NativeEventFilter *m_eventFilter = nullptr;
	QSet<int> m_vids;
	QEventLoop m_eventLoop;
	QSet<QProcess *> m_installs;
};
