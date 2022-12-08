#pragma once

#include <qapplication.h>
#include <qtcpserver.h>
#include <qmutex.h>
#include <thread>
#include <usbmuxd.h>
#include "driver-helper.h"

class TcpSocketWrapper;
class MediaSource;
class Application : public QApplication {
public:
	Application(int &argc, char **argv);
	~Application();

	DriverHelper &driverHelper() { return m_driverHelper; }
	static void usbmuxdDeviceEvent(const usbmuxd_event_t *event, void *user_data);
	QMap<QString, QPair<QString, uint32_t>> iOSDevices()
	{
		QMutexLocker locker(&m_devicesMutex);
		return m_iOSDevices;
	}

public slots:
	void onNewConnection();

private:
	void sendDeviceList(TcpSocketWrapper *wrapper, int type);
	void onMediaTask(const QJsonObject &req);

private:
	DriverHelper m_driverHelper;
	std::thread usbmuxd_th;
	QMap<QString, QPair<QString, uint32_t>> m_iOSDevices;
	QMutex m_devicesMutex;

	QTcpServer m_controlServer;
	QMap<int, MediaSource *> m_mediaSources;
};
