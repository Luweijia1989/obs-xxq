#pragma once

#include <qtcpsocket.h>
#include <qtcpserver.h>
#include <qpointer.h>
#include <qdebug.h>
#include <qjsondocument.h>
#include <qjsonobject.h>

enum class av_packet_type {
	FFM_PACKET_VIDEO,
	FFM_PACKET_AUDIO,
	FFM_MEDIA_VIDEO_INFO,
	FFM_MEDIA_AUDIO_INFO,
	FFM_MIRROR_STATUS,
};

enum class media_status { // same as obs_source_mirror_status
	Media_ERROR,
	Media_DEVICE_CONNECTED,
	Media_DEVICE_LOST,
	Media_START,
	Media_STOP,
	Media_OUTPUT,
	Media_AUDIO_SESSION_START,
};

#define FFM_SUCCESS 0
#define FFM_ERROR -1
#define FFM_UNSUPPORTED -2

struct av_packet_info {
	int64_t pts;
	uint32_t size;
	int serial;
	enum av_packet_type type;
};

struct media_audio_info {
	enum speaker_layout speakers;
	enum audio_format format;
	uint32_t samples_per_sec;
};

struct media_video_info {
	uint8_t video_extra[256];
	size_t video_extra_len;
};

enum class PhoneType {
	None,
	iOS,
	Android,
};

enum class MsgType {
	iOSDeviceList,
	AndroidDeviceList,
	DeviceListResult,
	MediaTask,
};

class TcpSocketWrapper : public QObject {
	Q_OBJECT
public:
	TcpSocketWrapper(QTcpSocket *socket) : m_socket(socket)
	{
		setParent(socket);
		connect(socket, &QTcpSocket::readyRead, this, [=]() {
			m_socketDataCache.append(socket->readAll());
			while (m_socketDataCache.size() > 4) {
				int payloadSize = 0;
				memcpy(&payloadSize, m_socketDataCache.data(), 4);
				if (m_socketDataCache.size() < payloadSize + 4)
					break;

				m_socketDataCache.remove(0, 4);
				QJsonDocument doc = QJsonDocument::fromJson(m_socketDataCache.left(payloadSize));
				m_socketDataCache.remove(0, payloadSize);
				if (!doc.isNull()) {
					auto obj = doc.object();
					emit msgReceived(obj);
				}
			}
		});
		connect(socket, &QTcpSocket::disconnected, this, [=]() { socket->deleteLater(); });
	}

	~TcpSocketWrapper() { qDebug() << "=========== TcpSocketWrapper destroyed."; }

	QTcpSocket *socket() { return m_socket; }
	void sendMsg(const QJsonDocument &msg)
	{
		if (m_socket->state() != QTcpSocket::ConnectedState)
			return;

		auto data = msg.toJson(QJsonDocument::Compact);
		int dataSize = data.size();

		QByteArray ret;
		ret.fill(0, 4);
		memcpy(ret.data(), &dataSize, sizeof(int));
		data.prepend(ret);

		m_socket->write(data);
	}

signals:
	void msgReceived(QJsonObject msg);

private:
	QByteArray m_socketDataCache;
	QTcpSocket *m_socket;
};

class MediaDataServer : public QObject {
	Q_OBJECT
public:
	MediaDataServer(QTcpServer *server) : m_server(server)
	{
		setParent(m_server);

		connect(m_server, &QTcpServer::newConnection, this, [=]() {
			if (!m_server->hasPendingConnections() || m_client)
				return;

			m_client = m_server->nextPendingConnection();
			connect(m_client, &QTcpSocket::disconnected, this, [=]() {
				m_mediaData.clear();
				m_client->deleteLater();
			});

			connect(m_client, &QTcpSocket::readyRead, this, [=]() {
				m_mediaData.append(m_client->readAll());
				while (m_mediaData.size() > sizeof(av_packet_info)) {
					av_packet_info header = {0};
					memcpy(&header, m_mediaData.data(), sizeof(av_packet_info));
					if (m_mediaData.size() < header.size + sizeof(av_packet_info))
						break;

					m_mediaData.remove(0, sizeof(av_packet_info));
					switch (header.type) {
					case av_packet_type::FFM_PACKET_VIDEO:
						emit mediaData((uint8_t *)m_mediaData.data(), header.size, header.pts, true);
						break;
					case av_packet_type::FFM_PACKET_AUDIO:
						emit mediaData((uint8_t *)m_mediaData.data(), header.size, header.pts, false);
						break;
					case av_packet_type::FFM_MEDIA_VIDEO_INFO: {
						media_video_info vInfo;
						memcpy(vInfo.video_extra, m_mediaData.data(), header.size);
						vInfo.video_extra_len = header.size;
						emit mediaVideoInfo(vInfo);
					} break;
					case av_packet_type::FFM_MEDIA_AUDIO_INFO: {
						media_audio_info aInfo;
						memcpy(&aInfo, m_mediaData.data(), sizeof(media_audio_info));
						emit mediaAudioInfo(aInfo);
					} break;
					case av_packet_type::FFM_MIRROR_STATUS: {
						int status = -1;
						memcpy(&status, m_mediaData.data(), sizeof(int));
						emit mediaStatus((media_status)status);
					} break;
					default:
						break;
					}
					m_mediaData.remove(0, header.size);
				}
			});
		});
	}

	quint16 port() { return m_server->serverPort(); }

signals:
	void mediaData(uint8_t *data, size_t size, int64_t pts, bool isVideo);
	void mediaVideoInfo(const media_video_info &info);
	void mediaAudioInfo(const media_audio_info &info);
	void mediaStatus(media_status status);

private:
	QTcpServer *m_server = nullptr;
	QPointer<QTcpSocket> m_client = nullptr;
	QByteArray m_mediaData;
};
