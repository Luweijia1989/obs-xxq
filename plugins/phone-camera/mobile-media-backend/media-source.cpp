#include "media-source.h"
#include <qtcpsocket.h>
#include <qhostaddress.h>
#include <qdebug.h>

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
	m_socket = new QTcpSocket;
	m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
	setParent(m_socket);
	connect(m_socket, &QTcpSocket::disconnected, m_socket, &QTcpSocket::deleteLater);
	m_socket->connectToHost(QHostAddress::LocalHost, port);
	bool ret = m_socket->waitForConnected(100);
	qDebug() << "MediaSource connect to media server, ret: " << (ret ? "success" : "fail");
	if (!ret)
		m_socket->deleteLater();
}

void MediaSource::setCurrentDevice(PhoneType type, QString deviceId)
{
	if (type != m_phoneType && m_mediaTask)
		m_mediaTask->deleteLater();

	m_phoneType = (PhoneType)type;
	if (type == PhoneType::iOS) {
		m_mediaTask = new iOSCamera(this);
	} else if (type == PhoneType::Android)
		m_mediaTask = new AndroidCamera(this);

	connect(m_mediaTask, &MediaTask::mediaData, this, [=](QByteArray data, int64_t timestamp, bool isVideo) {
		if (m_socket->state() != QTcpSocket::ConnectedState)
			return;

		if (isVideo) {
			if (timestamp == ((int64_t)UINT64_C(0x8000000000000000))) {
				av_packet_info info;
				info.type = av_packet_type::FFM_MEDIA_VIDEO_INFO;
				info.pts = timestamp;
				info.size = data.size();
				m_socket->write((char *)&info, sizeof(av_packet_info));
			} else {
				av_packet_info info;
				info.type = av_packet_type::FFM_PACKET_VIDEO;
				info.pts = timestamp;
				info.size = data.size();
				m_socket->write((char *)&info, sizeof(av_packet_info));
			}
			m_socket->write(data);
		} else {
			av_packet_info info;
			info.type = av_packet_type::FFM_PACKET_AUDIO;
			info.pts = timestamp;
			info.size = data.size();
			m_socket->write((char *)&info, sizeof(av_packet_info));
			m_socket->write(data);
		}
	});
	//connect(m_mediaTask, &MediaTask::mediaFinish, this, &PhoneCamera::onMediaFinish, Qt::DirectConnection);

	m_mediaTask->setExpectedDevice(deviceId);
	static_cast<Application *>(qApp)->driverHelper().checkDevices(m_phoneType);
}
