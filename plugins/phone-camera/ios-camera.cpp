#include "ios-camera.h"
#include <winsock.h>
#include <usbmuxd.h>
#include <qdebug.h>

#include <libimobiledevice/lockdown.h>

extern QSet<QString> runningDevices;

iOSCameraTaskThread::iOSCameraTaskThread(QObject *parent) : QThread(parent) {}

void iOSCameraTaskThread::run()
{
	uint32_t connectCount = 0;
	int fd = -1;
	std::vector<char> readBuf(1024 * 1024);
	uint32_t received = 0;
	circlebuf_init(&m_dataBuf);
	while (m_running) {
		if (fd < 0) {
			fd = usbmuxd_connect(m_deviceHandle, 2345);
			connectCount++;
			if (fd < 0) {
				if (connectCount > 5) {
					emit connectResult(m_udid, false);
					break;
				}
				QThread::msleep(50);
				continue;
			} else {
				emit connectResult(m_udid, true);
			}
		} else {
			int ret = usbmuxd_recv_timeout(fd, readBuf.data(), 1024 * 1024, &received, 100);
			if (ret == 0) {
				circlebuf_push_back(&m_dataBuf, readBuf.data(), received);
				parseMediaData();
			} else if (ret == -ETIMEDOUT) {

			} else {
				qDebug() << "read device data error";
				break;
			}
		}
	}
	circlebuf_free(&m_dataBuf);
	if (fd >= 0)
		usbmuxd_disconnect(fd);

	emit mediaFinish();
}

void iOSCameraTaskThread::startByInfo(QString udid, uint32_t deviceHandle)
{
	m_running = true;
	m_deviceHandle = deviceHandle;
	m_udid = udid;

	start();
}

void iOSCameraTaskThread::stopTask()
{
	m_running = false;
	wait();
}

void iOSCameraTaskThread::parseMediaData()
{
	auto headerLength = sizeof(_PortalFrame);
	while (true) {
		if (m_dataBuf.size < headerLength)
			break;

		_PortalFrame frame = {0};
		circlebuf_peek_front(&m_dataBuf, &frame, headerLength);

		frame.version = ntohl(frame.version);
		frame.type = ntohl(frame.type);
		frame.tag = ntohl(frame.tag);
		frame.payloadSize = ntohl(frame.payloadSize);

		if (headerLength + frame.payloadSize > m_dataBuf.size)
			break;

		circlebuf_pop_front(&m_dataBuf, NULL, headerLength);

		uint8_t *payload = (uint8_t *)malloc(frame.payloadSize);
		circlebuf_pop_front(&m_dataBuf, payload, frame.payloadSize);
		emit mediaData(payload, frame.payloadSize, frame.type == 101);
		free(payload);
	}
}

iOSCamera::iOSCamera(QObject *parent) : QObject(parent), m_taskThread(new iOSCameraTaskThread(this))
{
	connect(this, &iOSCamera::updateDeviceList, this, &iOSCamera::onUpdateDeviceList);

	m_updateDeviceThread = new QThread;
	m_updateTimer = new QTimer();
	m_updateTimer->setSingleShot(false);
	m_updateTimer->moveToThread(m_updateDeviceThread);

	QObject::connect(m_updateTimer, &QTimer::timeout, m_updateTimer, [=] {
		QMap<QString, QPair<QString, uint32_t>> list;
		usbmuxd_device_info_t *devices = NULL;
		auto deviceCount = usbmuxd_get_device_list(&devices);
		if (deviceCount > 0) {
			for (size_t i = 0; i < deviceCount; i++) {
				auto device = devices[i];
				if (device.conn_type == CONNECTION_TYPE_NETWORK)
					continue;

				QString udid(device.udid);
				QString name = getDeviceName(udid);
				list.insert(udid, {name, device.handle});
			}
		}
		usbmuxd_device_list_free(&devices);

		emit updateDeviceList(list);
	});

	connect(m_taskThread, &iOSCameraTaskThread::connectResult, this, [this](QString udid, bool success) {
		if (!success) {
			m_taskThread->stopTask();
			m_state = UnConnected;
		} else {
			m_connectedDevice = udid;
			runningDevices.insert(udid);
			m_state = Connected;
		}
	});
	connect(m_taskThread, &iOSCameraTaskThread::mediaData, this, &iOSCamera::mediaData, Qt::DirectConnection);
	connect(m_taskThread, &iOSCameraTaskThread::mediaFinish, this, &iOSCamera::mediaFinish, Qt::DirectConnection);
}

iOSCamera::~iOSCamera()
{
	stop();

	m_updateTimer->deleteLater();
	m_updateDeviceThread->deleteLater();
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

	if (LOCKDOWN_E_SUCCESS != lockdownd_client_new(lockdown_device, &lockdown, "usbmuxd")) {
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
	if (m_updateDeviceThread->isRunning())
		return;

	m_updateDeviceThread->start();
	QMetaObject::invokeMethod(m_updateTimer, "start", Q_ARG(int, 500));
}

void iOSCamera::stop()
{
	m_taskThread->stopTask();

	if (!m_updateDeviceThread->isRunning())
		return;

	m_updateDeviceThread->quit();
	m_updateDeviceThread->wait();
}

void iOSCamera::setCurrentDevice(QString udid)
{
	m_currentDevice = udid;
}

void iOSCamera::onUpdateDeviceList(QMap<QString, QPair<QString, uint32_t>> devices)
{
	if (m_state == Connected) {
		// 如果当前设备列表中不包含当前连接的设备（连接设备被移除）
		// 或者用户更改了想要连接的设备，先把旧任务停掉
		if (!devices.contains(m_connectedDevice) || (!m_currentDevice.isEmpty() && m_currentDevice != m_connectedDevice)) {
			m_taskThread->stopTask();

			m_state = UnConnected;
			runningDevices.remove(m_connectedDevice);
		}
	} else {
		for (auto iter = devices.begin(); iter != devices.end(); iter++) {
			if (runningDevices.contains(iter.key()))
				continue;

			if (m_currentDevice.isEmpty() || m_currentDevice == iter.key()) {
				// try connect
				if (m_state != Connecting) {
					m_taskThread->startByInfo(iter.key(), iter.value().second);
				}
			}
		}
	}
}
