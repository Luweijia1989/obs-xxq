#include "ios-screen-mirror-task-thread.h"
#include <qdebug.h>
#include <qelapsedtimer.h>
#include <util/circlebuf.h>
#include "ios/usbmuxd/usbmuxd-proto.h"
#include <plist/plist.h>
#include "ios/screenmirror/packet.h"

iOSScreenMirrorTaskThread::iOSScreenMirrorTaskThread(QObject *parent) : iOSTask(parent) {}

void iOSScreenMirrorTaskThread::sendCmd(bool isStart)
{
	QString tid = m_udid.replace('-', "").toUpper();

	auto data = usbmuxdTaskCMD(tid, isStart);
	m_mirrorSocket->write(data.data(), data.size(), isStart ? 0 : 100);
}

void iOSScreenMirrorTaskThread::startTask(QString udid, uint32_t deviceHandle)
{
	iOSTask::startTask(udid, deviceHandle);

	m_mediaCache.clear();
	m_mirrorSocket = new TcpClient();
	connect(m_mirrorSocket, &TcpClient::disconnected, this, &iOSTask::finished);
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
	if (m_mirrorSocket->connectToHost("127.0.0.1", USBMUXD_SOCKET_PORT, 100))
		sendCmd(true);
	else {
		qDebug() << "error in connect to usbmuxd exe";
		emit finished();
	}
}

void iOSScreenMirrorTaskThread::stopTask()
{
	if (m_mirrorSocket) {
		sendCmd(false);
		delete m_mirrorSocket;
		m_mirrorSocket = nullptr;
	}
	iOSTask::stopTask();
}
