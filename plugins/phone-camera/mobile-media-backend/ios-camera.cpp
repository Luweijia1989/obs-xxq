#include "ios-camera.h"
#include <winsock.h>
#include <usbmuxd.h>
#include <qdebug.h>
#include <util/platform.h>
#include <libimobiledevice/lockdown.h>

#include "application.h"

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
					break;
				}
				QThread::msleep(50);
				continue;
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

		uint8_t *payload = (uint8_t *)bmalloc(frame.payloadSize);
		circlebuf_pop_front(&m_dataBuf, payload, frame.payloadSize);
		emit mediaData(QByteArray((char *)payload, frame.payloadSize), os_gettime_ns(), frame.type == 101);
		bfree(payload);
	}
}

iOSCamera::iOSCamera(QObject *parent) : MediaTask(parent), m_taskThread(new iOSCameraTaskThread(this))
{
	connect(&m_scanTimer, &QTimer::timeout, this, &iOSCamera::onUpdateDeviceList);
	connect(m_taskThread, &iOSCameraTaskThread::mediaData, this, &MediaTask::mediaData, Qt::DirectConnection);
	connect(m_taskThread, &iOSCameraTaskThread::mediaFinish, this, &MediaTask::mediaFinish, Qt::DirectConnection);
	connect(m_taskThread, &iOSCameraTaskThread::finished, this, &iOSCamera::stopTask);
}

iOSCamera::~iOSCamera()
{
	stopTask();
}

void iOSCamera::startTask(QString device, uint32_t handle)
{
	MediaTask::startTask(device, handle);

	m_taskThread->startByInfo(device, handle);
}

void iOSCamera::stopTask()
{
	m_taskThread->stopTask();

	MediaTask::stopTask();
}

void iOSCamera::onUpdateDeviceList()
{
	auto devices = static_cast<Application *>(qApp)->iOSDevices();
	for (auto iter = devices.begin(); iter != devices.end(); iter++) {
		if (runningDevices.contains(iter.key()))
			continue;

		if (m_expectedDevice == "auto" || m_expectedDevice == iter.key()) {
			// try connect
			startTask(iter.key(), iter.value().second);
			break;
		}
	}
}
