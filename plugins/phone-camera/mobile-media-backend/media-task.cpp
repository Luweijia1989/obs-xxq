#include "media-task.h"
#include "application.h"
#include "ios/screenmirror/packet.h"
#include "c-util.h"

extern QSet<QString> runningDevices;
extern QSet<QString> AndroidDevices;

static QString serialNumber(QString path)
{
	path = path.left(path.lastIndexOf('#'));
	return path.mid(path.lastIndexOf('#') + 1);
}

MediaTask::MediaTask(PhoneType type, QObject *parent) : QObject(parent), m_phoneType(type)
{
	m_scanTimer.setInterval(100);
	m_scanTimer.setSingleShot(false);
	connect(&m_scanTimer, &QTimer::timeout, this, &MediaTask::onScanTimerTimeout);

	m_usbmuxdConnectTimer.setInterval(100);
	m_usbmuxdConnectTimer.setSingleShot(false);
	connect(&m_usbmuxdConnectTimer, &QTimer::timeout, this, [=]() {
		int fd = usbmuxd_connect(m_iOSDeviceHandle, 2345);
		if (fd == -ENODEV)
			stopTask(false);
		else if (fd > 0) {
			m_usbmuxdConnectTimer.stop();
			m_usbmuxdFD = fd;
			add_exceptions(m_finalId.toUtf8().data());
			add_media_callback(m_finalId.toUtf8().data(), MediaTask::deviceData, MediaTask::deviceLost, this);
		}
	});
}

MediaTask::~MediaTask()
{
	stopTask(true);
}

void MediaTask::setExpectedDevice(QString device)
{
	do {
		if (m_expectedDevice.isEmpty())
			break;

		if (device == "auto")
			return;
		else {
			if (m_expectedDevice == "auto") //todo check auto device is same with target device
				break;
			else {
				if (m_expectedDevice == device)
					return;
				else
					break;
			}
		}
	} while (1);

	stopTask(true);
	m_expectedDevice = device;
	m_scanTimer.start();
}

void MediaTask::startTask(QString path, uint32_t handle)
{
	if (m_taskStarted)
		return;

	qDebug() << "MediaTask start ====> " << path;

	m_scanTimer.stop();
	m_connectedDevice = path;
	m_finalId = path;
	m_iOSDeviceHandle = handle;
	switch (m_phoneType) {
	case PhoneType::iOS:
	case PhoneType::iOSCamera:
		m_finalId.replace('-', "").toUpper();
		break;
	case PhoneType::Android:
		m_finalId = serialNumber(m_connectedDevice);
		break;
	default:
		break;
	}
	runningDevices.insert(path);

	m_mediaCache.clear();
	if (m_phoneType == PhoneType::iOSCamera) {
		m_usbmuxdConnectTimer.start();
	} else if (m_phoneType == PhoneType::iOS || m_phoneType == PhoneType::Android)
		add_media_callback(m_finalId.toUtf8().data(), MediaTask::deviceData, MediaTask::deviceLost, this);

	m_taskStarted = true;
}

void MediaTask::stopTask(bool finalStop)
{
	if (!m_taskStarted)
		return;

	qDebug() << "MediaTask stop...";

	if (m_usbmuxdConnectTimer.isActive())
		m_usbmuxdConnectTimer.stop();

	if (m_usbmuxdFD) {
		usbmuxd_disconnect(m_usbmuxdFD);
		m_usbmuxdFD = -1;
	}

	remove_media_callback(m_finalId.toUtf8().data());

	runningDevices.remove(m_connectedDevice);
	m_connectedDevice = QString();
	if (!finalStop)
		m_scanTimer.start();

	m_taskStarted = false;
}

void MediaTask::onScanTimerTimeout()
{
	if (m_phoneType == PhoneType::Android) {
		for (auto iter = AndroidDevices.begin(); iter != AndroidDevices.end(); iter++) {
			if (runningDevices.contains(*iter))
				continue;

			if (m_expectedDevice != "auto" && !(*iter).contains(serialNumber(m_expectedDevice)))
				continue;

			startTask(*iter);
			break;
		}
	} else if (m_phoneType == PhoneType::iOS || m_phoneType == PhoneType::iOSCamera) {
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
}

void MediaTask::deviceDataInternal(char *buf, int size)
{
	if (m_phoneType == PhoneType::iOSCamera) // todo
		return;

	m_mediaCache.append(buf, size);
	while (true) {
		int headerSize = sizeof(media_header);
		if (m_mediaCache.size() < headerSize)
			break;

		media_header header = {0};
		memcpy(&header, m_mediaCache.data(), headerSize);
		if (m_mediaCache.size() < headerSize + header.payload_size)
			break;

		auto media = m_mediaCache.mid(headerSize, header.payload_size);
		if (header.type == 2) {
			int type = 0;
			memcpy(&type, media.data(), sizeof(int));
			emit mediaState(type == 0);
		} else
			emit mediaData(media.data(), media.size(), header.timestamp, header.type == 0);
		m_mediaCache.remove(0, headerSize + header.payload_size);
	}
}

void MediaTask::deviceData(char *buf, int size, void *cb_data)
{
	MediaTask *s = (MediaTask *)cb_data;
	s->deviceDataInternal(buf, size);
}

void MediaTask::deviceLost(void *cb)
{
	MediaTask *s = (MediaTask *)cb;
	QMetaObject::invokeMethod(s, [=]() { s->stopTask(false); });
}
