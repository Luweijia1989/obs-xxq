#include "android-camera.h"
#include <qdebug.h>
#include <qelapsedtimer.h>
#include "ios/screenmirror/packet.h"

extern QSet<QString> runningDevices;
extern QSet<QString> AndroidDevices;

static QString serialNumber(QString path)
{
	path = path.left(path.lastIndexOf('#'));
	return path.mid(path.lastIndexOf('#') + 1);
}

AndroidCamera::AndroidCamera(QObject *parent) : MediaTask(parent)
{
	connect(&m_scanTimer, &QTimer::timeout, this, [=]() {
		for (auto iter = AndroidDevices.begin(); iter != AndroidDevices.end(); iter++) {
			if (runningDevices.contains(*iter))
				continue;

			if (m_expectedDevice != "auto" && !(*iter).contains(serialNumber(m_expectedDevice)))
				continue;

			startTask(*iter);
			break;
		}
	});
}

AndroidCamera::~AndroidCamera()
{
	stopTask(true);
}

void AndroidCamera::startTask(QString path, uint32_t handle)
{
	Q_UNUSED(handle)

	MediaTask::startTask(path);

	m_mediaCache.clear();
	m_mirrorSocket = new TcpClient();
	connect(m_mirrorSocket, &TcpClient::disconnected, this, [=] { stopTask(false); });
	connect(m_mirrorSocket, &TcpClient::onData, this,
		[=](uint8_t *data, int size) {
			m_mediaCache.append((char *)data, size);
			while (true) {
				if (m_mediaCache.size() < sizeof(media_header))
					break;

				media_header header = {0};
				memcpy(&header, m_mediaCache.data(), sizeof(media_header));
				if (m_mediaCache.size() < sizeof(media_header) + header.payload_size)
					break;

				auto media = m_mediaCache.mid(sizeof(media_header), header.payload_size);
				emit mediaData(media, header.timestamp, header.type == 0);
				m_mediaCache.remove(0, sizeof(media_header) + header.payload_size);
			}
		},
		Qt::DirectConnection);
	if (m_mirrorSocket->connectToHost("127.0.0.1", USBMUXD_SOCKET_PORT, 100)) {
		auto data = usbmuxdTaskCMD(serialNumber(m_connectedDevice), true);
		m_mirrorSocket->write(data.data(), data.size(), 0);
	} else {
		qDebug() << "error in connect to usbmuxd exe";
		stopTask(false);
	}
}

void AndroidCamera::stopTask(bool finalStop)
{
	if (m_mirrorSocket) {
		auto data = usbmuxdTaskCMD(serialNumber(m_connectedDevice), false);
		m_mirrorSocket->write(data.data(), data.size(), 100);
		delete m_mirrorSocket;
		m_mirrorSocket = nullptr;
	}

	MediaTask::stopTask(finalStop);
}
