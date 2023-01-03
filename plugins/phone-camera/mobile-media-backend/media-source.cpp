#include "media-source.h"
#include <qtcpsocket.h>
#include <qhostaddress.h>
#include <qdebug.h>
#include <media-io/audio-io.h>

#include "application.h"
#include "ios-camera.h"
#include "android-camera.h"

MediaSource::MediaSource(QObject *parent) : QObject(parent) {}

MediaSource::~MediaSource()
{
	qDebug() << "MediaSource destroyed.";
}

void MediaSource::connectToHost(int port)
{
	m_socket = new TcpClient(this);
	connect(m_socket, &TcpClient::disconnected, this, [=]() {
		qDebug() << "MediaSource socket disconnected.";
		App()->mediaObjectRegister(m_phoneType, this, false);
		deleteLater();
	});
	bool res = m_socket->connectToHost("127.0.0.1", port, 100);
	qDebug() << "MediaSource connect to media server, ret: " << (res ? "success" : "fail");
	if (!res)
		deleteLater();
}

void MediaSource::setCurrentDevice(PhoneType type, QString deviceId)
{
	if (type != m_phoneType && m_mediaTask)
		m_mediaTask->deleteLater();

	App()->mediaObjectRegister(type, this, true);
	m_phoneType = (PhoneType)type;
	if (type == PhoneType::iOS) {
		m_mediaTask = new iOSCamera(this);
	} else if (type == PhoneType::Android)
		m_mediaTask = new AndroidCamera(this);

	connect(m_mediaTask, &MediaTask::mediaData, this,
		[=](char *data, int size, int64_t timestamp, bool isVideo) {
			if (isVideo) {
				if (timestamp == ((int64_t)UINT64_C(0x8000000000000000))) {
					av_packet_info info;
					info.type = av_packet_type::FFM_MEDIA_VIDEO_INFO;
					info.pts = timestamp;
					info.size = size;
					m_socket->write((char *)&info, sizeof(av_packet_info), 0);
				} else {
					av_packet_info info;
					info.type = av_packet_type::FFM_PACKET_VIDEO;
					info.pts = timestamp;
					info.size = size;
					m_socket->write((char *)&info, sizeof(av_packet_info), 0);
				}
				m_socket->write(data, size, 0);
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
					m_socket->write((char *)&info, sizeof(av_packet_info), 0);
					m_socket->write((char *)&audio_info, sizeof(media_audio_info), 0);

				} else {
					av_packet_info info;
					info.type = av_packet_type::FFM_PACKET_AUDIO;
					info.pts = timestamp;
					info.size = size;
					m_socket->write((char *)&info, sizeof(av_packet_info), 0);
					m_socket->write(data, size, 0);
				}
			}
		},
		Qt::DirectConnection);
	connect(m_mediaTask, &MediaTask::mediaState, this,
		[=](bool start) {
			av_packet_info info;
			info.type = av_packet_type::FFM_MIRROR_STATUS;
			info.size = sizeof(int);
			info.pts = 0;
			m_socket->write((char *)&info, sizeof(av_packet_info), 0);
			int type = start ? 3 : 4;
			m_socket->write((char *)&type, sizeof(int), 0);
		},
		Qt::DirectConnection);

	m_mediaTask->setExpectedDevice(deviceId);
	App()->driverHelper().checkDevices();
}
