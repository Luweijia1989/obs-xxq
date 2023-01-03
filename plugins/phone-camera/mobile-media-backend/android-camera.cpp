#include "android-camera.h"
#include <qdebug.h>
#include <qelapsedtimer.h>
#include "ios/screenmirror/packet.h"
#include "c-util.h"

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

void AndroidCamera::mirrorDataInternal(char *buf, int size)
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

void AndroidCamera::mirrorData(char *buf, int size, void *cb_data)
{
	AndroidCamera *s = (AndroidCamera *)cb_data;
	s->mirrorDataInternal(buf, size);
}

void AndroidCamera::deviceLost(void *cb)
{
	AndroidCamera *s = (AndroidCamera *)cb;
	QMetaObject::invokeMethod(s, [=]() { s->stopTask(false); });
}

void AndroidCamera::startTask(QString path, uint32_t handle)
{
	Q_UNUSED(handle)

	MediaTask::startTask(path);
	m_mediaCache.clear();

	add_media_callback(serialNumber(m_connectedDevice).toUtf8().data(), AndroidCamera::mirrorData, AndroidCamera::deviceLost,this);
	qDebug() << "Android camera startTask";
}

void AndroidCamera::stopTask(bool finalStop)
{
	remove_media_callback(serialNumber(m_connectedDevice).toUtf8());
	MediaTask::stopTask(finalStop);
}
