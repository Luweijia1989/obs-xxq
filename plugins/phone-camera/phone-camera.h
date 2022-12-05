#pragma once

#include <pthread.h>
#include <QString>
#include <QDebug>
#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qfile.h>

#include "usb-helper.h"
#include <obs.h>

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

class iOSCamera;
class AndroidCamera;
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
	void onMediaData(uint8_t *data, size_t size, int64_t timestamp,  bool isVideo);
	void onMediaFinish();

private:
	QFile m_videodump;

	/***************************/
	PhoneType m_phoneType = PhoneType::None;
	iOSCamera *m_iOSCamera = nullptr;
	AndroidCamera *m_androidCamera = nullptr;
	QMap<QString, QPair<QString, uint32_t>> m_iOSDevices;

	obs_source_t *m_source;
	Decoder *m_audioDecoder = nullptr;
	Decoder *m_videoDecoder = nullptr;
	obs_source_frame2 frame = {0};
};
