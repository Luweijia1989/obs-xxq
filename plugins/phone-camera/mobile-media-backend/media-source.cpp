#include "media-source.h"
#include <qtcpsocket.h>
#include <qhostaddress.h>
#include <qdebug.h>
#include <media-io/audio-io.h>

#include "application.h"

MediaSource::MediaSource(QObject *parent) : QObject(parent) {}

MediaSource::~MediaSource()
{
	qDebug() << "MediaSource destroyed.";
}

void MediaSource::connectToMediaTarget(QString name)
{
	ipc_client_create(&m_ipcClient, name.toUtf8().data());
}
extern "C" {
extern int64_t os_gettime_ns();
}
void MediaSource::setCurrentDevice(PhoneType type, QString deviceId)
{
	if (type != m_phoneType && m_mediaTask)
		m_mediaTask->deleteLater();

	App()->mediaObjectRegister(type, this, true);
	m_phoneType = (PhoneType)type;
	m_mediaTask = new MediaTask(m_phoneType, this);

	connect(
		m_mediaTask, &MediaTask::mediaData, this,
		[=](char *data, int size, int64_t timestamp, bool isVideo) {
			int res = 0;
			if (isVideo) {
				av_packet_info info;
				info.type = timestamp == ((int64_t)UINT64_C(0x8000000000000000)) ? av_packet_type::FFM_MEDIA_VIDEO_INFO
												 : av_packet_type::FFM_PACKET_VIDEO;
				info.pts = timestamp;
				info.size = size;
				res = ipc_client_write_2(m_ipcClient, (char *)&info, sizeof(av_packet_info), data, size, 0);
			} else {
				if (timestamp == ((int64_t)UINT64_C(0x8000000000000000))) {
					uint32_t *ud = (uint32_t *)data;
					media_audio_info audio_info;
					audio_info.format = (enum audio_format)ud[0];
					audio_info.samples_per_sec = ud[1];
					audio_info.speakers = (enum speaker_layout)ud[2];

					av_packet_info info;
					info.type = av_packet_type::FFM_MEDIA_AUDIO_INFO;
					info.pts = timestamp;
					info.size = sizeof(media_audio_info);
					res = ipc_client_write_2(m_ipcClient, (char *)&info, sizeof(av_packet_info), (char *)&audio_info,
								 sizeof(media_audio_info), 0);

				} else {
					av_packet_info info;
					info.type = av_packet_type::FFM_PACKET_AUDIO;
					info.pts = timestamp;
					info.size = size;
					res = ipc_client_write_2(m_ipcClient, (char *)&info, sizeof(av_packet_info), data, size, 0);
				}
			}
			if (res == -1 && !m_emitError) {
				m_emitError = true;
				QMetaObject::invokeMethod(this, "mediaTargetLost");
			}
		},
		Qt::DirectConnection);
	connect(
		m_mediaTask, &MediaTask::mediaState, this,
		[=](bool start) {
			av_packet_info info;
			info.type = av_packet_type::FFM_MIRROR_STATUS;
			info.size = sizeof(int);
			info.pts = 0;
			int type = start ? 3 : 4;
			int res = ipc_client_write_2(m_ipcClient, (char *)&info, sizeof(av_packet_info), (char *)&type, sizeof(int), 0);
			if (res == -1 && !m_emitError) {
				m_emitError = true;
				QMetaObject::invokeMethod(this, "mediaTargetLost");
			}
		},
		Qt::DirectConnection);

	m_mediaTask->setExpectedDevice(deviceId);
	App()->driverHelper().checkDevices();
}

void MediaSource::mediaTargetLost()
{
	qDebug() << "MediaSource target lost.";
	App()->mediaObjectRegister(m_phoneType, this, false);
	deleteLater();
}
