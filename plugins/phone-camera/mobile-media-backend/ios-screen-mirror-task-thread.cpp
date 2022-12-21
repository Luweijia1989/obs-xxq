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

	plist_t dict = plist_new_dict();
	plist_dict_set_item(dict, "CmdType", plist_new_string(isStart ? "Start" : "Stop"));
	plist_dict_set_item(dict, "DeviceId", plist_new_string(tid.toUtf8().data()));

	int res = -1;
	char *xml = NULL;
	uint32_t xmlsize = 0;
	plist_to_xml(dict, &xml, &xmlsize);
	if (xml) {
		struct usbmuxd_header hdr;
		auto size = sizeof(hdr) + xmlsize;
		hdr.version = 1;
		hdr.length = size;
		hdr.message = MESSAGE_CUSTOM;
		hdr.tag = 1;
		auto sendBuffer = (uint8_t *)malloc(size);
		memcpy(sendBuffer, &hdr, sizeof(hdr));
		memcpy(sendBuffer + sizeof(hdr), xml, xmlsize);
		m_mirrorSocket->send((char *)sendBuffer, size);
		free(sendBuffer);
		free(xml);
	}
	plist_free(dict);

	if (!isStart)
		m_mirrorSocket->close();
}

void iOSScreenMirrorTaskThread::startTask(QString udid, uint32_t deviceHandle)
{
	iOSTask::startTask(udid, deviceHandle);

	m_mediaCache.clear();
	m_mirrorSocket = new TcpClient();
	connect(m_mirrorSocket, &TcpClient::finished, this, &iOSTask::finished);
	connect(m_mirrorSocket, &TcpClient::connected, this, [=]() { sendCmd(true); }, Qt::DirectConnection);
	connect(m_mirrorSocket, &TcpClient::onData, this,
		[=](char *data, int size) {
			m_mediaCache.append(data, size);
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
	m_mirrorSocket->connectToHost("127.0.0.1", USBMUXD_SOCKET_PORT);
}

void iOSScreenMirrorTaskThread::stopTask()
{
	if (m_mirrorSocket) {
		m_mirrorSocket->deleteLater();
		m_mirrorSocket = nullptr;
	}
	iOSTask::stopTask();
}
