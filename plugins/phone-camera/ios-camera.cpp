#include "ios-camera.h"
#include <usbmuxd.h>
#include <qdebug.h>

#include <libimobiledevice/lockdown.h>

iOSCamera::iOSCamera(QObject *parent) : QObject(parent) {}

iOSCamera::~iOSCamera()
{
	if (m_updateDeviceThread) {
		m_updateDeviceThread->quit();
		m_updateDeviceThread->wait();
		m_updateTimer->deleteLater();
		m_updateDeviceThread->deleteLater();
	}
}

QString iOSCamera::getDeviceName(QString udid)
{
	QString result;

	idevice_t lockdown_device = NULL;
	idevice_new_with_options(&lockdown_device, udid.toUtf8().data(), IDEVICE_LOOKUP_USBMUX);

	if (!lockdown_device) {
		return result;
	}

	lockdownd_client_t lockdown = NULL;

	if (LOCKDOWN_E_SUCCESS != lockdownd_client_new(lockdown_device, &lockdown, "obs-ios-camera-plugin")) {
		idevice_free(lockdown_device);
		return result;
	}

	char *lockdown_device_name = NULL;

	if ((LOCKDOWN_E_SUCCESS != lockdownd_get_device_name(lockdown, &lockdown_device_name)) || !lockdown_device_name) {
		idevice_free(lockdown_device);
		lockdownd_client_free(lockdown);
		return result;
	}

	idevice_free(lockdown_device);
	lockdownd_client_free(lockdown);

	result = lockdown_device_name;
	free(lockdown_device_name);

	return result;
}

void iOSCamera::start()
{
	m_updateDeviceThread = new QThread;
	m_updateTimer = new QTimer();
	m_updateTimer->moveToThread(m_updateDeviceThread);
	m_updateDeviceThread->start();
	m_updateTimer->setSingleShot(false);
	QObject::connect(m_updateTimer, &QTimer::timeout, m_updateTimer, [=] {
		m_devices.clear();
		QMap<QString, QString> list;
		usbmuxd_device_info_t *devices = NULL;
		auto deviceCount = usbmuxd_get_device_list(&devices);
		if (deviceCount > 0) {
			for (size_t i = 0; i < deviceCount; i++) {
				auto device = devices[i];
				if (device.conn_type == CONNECTION_TYPE_NETWORK)
					continue;

				QString udid(device.udid);
				m_devices.insert(udid, device.handle);
				QString name = getDeviceName(udid);
				list.insert(udid, name);
			}
		}
		usbmuxd_device_list_free(&devices);

		emit updateDeviceList(list);
	});
	QMetaObject::invokeMethod(m_updateTimer, "start", Q_ARG(int, 500));
}
