#include "ios-screen-mirror-task-thread.h"
#include <qdebug.h>
#include <qelapsedtimer.h>
#include <util/circlebuf.h>
#include "ios/usbmuxd/usbmuxd-proto.h"
#include <plist/plist.h>
#include "ios/screenmirror/packet.h"
#include "c-util.h"

iOSScreenMirrorTaskThread::iOSScreenMirrorTaskThread(QObject *parent) : iOSTask(parent) {}

void iOSScreenMirrorTaskThread::startTask(QString udid, uint32_t deviceHandle)
{
	iOSTask::startTask(udid, deviceHandle);
	m_udid.replace('-', "").toUpper();
	m_mediaCache.clear();
	add_media_callback(m_udid.toUtf8().data(), iOSScreenMirrorTaskThread::mirrorData, iOSScreenMirrorTaskThread::deviceLost, this);
}

void iOSScreenMirrorTaskThread::mirrorDataInternal(char *buf, int size)
{
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

void iOSScreenMirrorTaskThread::mirrorData(char *buf, int size, void *cb_data)
{
	iOSScreenMirrorTaskThread *s = (iOSScreenMirrorTaskThread *)cb_data;
	s->mirrorDataInternal(buf, size);
}

void iOSScreenMirrorTaskThread::deviceLost(void *cb_data)
{
	iOSScreenMirrorTaskThread *s = (iOSScreenMirrorTaskThread *)cb_data;
	QMetaObject::invokeMethod(s, [=]() { emit s->finished(); });
}

void iOSScreenMirrorTaskThread::stopTask()
{
	remove_media_callback(m_udid.toUtf8());
	iOSTask::stopTask();
}
