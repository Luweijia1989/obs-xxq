#pragma once

#include <QString>
#include <QDebug>
#include <qpointer.h>
#include <qfile.h>
#include <obs.h>
#include <qtcpsocket.h>
#include <qeventloop.h>

#include "common.h"

extern "C" {
#include "ffmpeg-decode.h"
}

class Decoder {
	struct ffmpeg_decode decode;

public:
	inline Decoder() { memset(&decode, 0, sizeof(decode)); }
	inline ~Decoder() { ffmpeg_decode_free(&decode); }

	inline operator ffmpeg_decode *() { return &decode; }
	inline ffmpeg_decode *operator->() { return &decode; }
};

class PhoneCamera : public QObject {
	Q_OBJECT
public:
	PhoneCamera(obs_data_t *settings, obs_source_t *source);
	~PhoneCamera();

	PhoneType type() { return m_phoneType; }
	QMap<QString, QPair<QString, uint32_t>> deviceList(int type);
	const obs_source_t *source() const { return m_source; }

public slots:
	void switchPhoneType();
	void taskEnd();
	void onMediaVideoInfo(const media_video_info &info);
	void onMediaData(uint8_t *data, size_t size, int64_t timestamp, bool isVideo);
	void onMediaFinish();

private:
	QFile m_videodump;

	/***************************/
	PhoneType m_phoneType = PhoneType::None;

	QMap<QString, QPair<QString, uint32_t>> m_devices;

	obs_source_t *m_source = nullptr;
	Decoder m_audioDecoder;
	Decoder m_videoDecoder;
	obs_source_frame2 frame = {0};

	TcpSocketWrapper *m_socketWrapper = nullptr;
	QPointer<MediaDataServer> m_mediaDataServer = nullptr;
	QEventLoop m_blockEvent;
};
