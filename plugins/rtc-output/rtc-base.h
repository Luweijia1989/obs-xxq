#pragma once

#include <obs-module.h>
#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRect>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include "rtc-define.h"

typedef std::function< void(int type, QJsonObject data) > RtcEventCallback;

class RTCBase : public QObject {
	Q_OBJECT
public:
	struct VideoInfo {
		int audioBitrate;
		int videoBitrate;
		int fps;
		int width;
		int height;
		VideoInfo()
		{
			audioBitrate = 0;
			videoBitrate = 0;
			fps = 0;
			width = 0;
			height = 0;
		}
	};

	virtual void init() = 0;
	virtual void enterRoom() = 0;
	virtual void exitRoom() = 0;
	virtual void setRemoteViewHwnd(long window) = 0;
	virtual void sendAudio(struct audio_data *data) = 0;
	virtual void sendVideo(struct video_data *data) = 0;
	virtual void setSei(const QJsonObject &data, int insetType) = 0;
	virtual void setAudioInputDevice(const QString &deviceId) = 0;
	virtual void setAudioInputMute(bool mute) = 0;
	virtual void setAudioInputVolume(int volume) = 0;
	virtual void setAudioOutputDevice(const QString &deviceId) = 0;
	void setCropInfo(int x, int cropWidth)
	{
		m_cropInfo = QRect(x, 0, cropWidth, m_vinfo.height);
	}

	const QRect &cropInfo() { return m_cropInfo; }

	void setVideoInfo(int a, int v, int fps, int w, int h)
	{
		m_vinfo.audioBitrate = a;
		m_vinfo.videoBitrate = v;
		m_vinfo.fps = fps;
		m_vinfo.width = w;
		m_vinfo.height = h;
	}

	const VideoInfo &videoInfo() { return m_vinfo; }

	void setLinkInfo(QString str)
	{
		QJsonDocument jd = QJsonDocument::fromJson(str.toUtf8());
		QJsonObject obj = jd.object();
		link_rtc_uid = obj["rtcUserId"].toString();
		link_rtc_otherUid = obj["rtcOtherUserId"].toString();
		link_uid = obj["uid"].toString();
		link_otherUid = obj["otherUid"].toString();
		link_rtcRoomId = obj["rtcRoomId"].toString();
		link_streamUrl = obj["streamUrl"].toString();
		link_streamId = obj["streamId"].toString();
		link_cdnSupplier = obj["cdnSupplier"].toString();
		link_rtcRoomToken = obj["rtcRoomToken"].toString();
		link_std_rtcRoomId = link_rtcRoomId.toStdString();
		link_rtcAPPID = obj["rtcAppId"].toString().toInt();
		link_cdnAPPID = obj["cdnAppId"].toString().toInt();
		link_cdnBizID = obj["cdnBizId"].toString().toInt();
		link_std_rtcUid = link_rtc_uid.toStdString();
		link_std_rtcRoomToken = link_rtcRoomToken.toStdString();
		link_type = obj["linkType"].toInt();
		is_video_link = link_type == 0;
	}

	void setMicInfo(QString str)
	{
		QJsonDocument jd = QJsonDocument::fromJson(str.toUtf8());
		QJsonObject obj = jd.object();
		setAudioInputDevice(obj["device"].toString());
		setAudioInputMute(obj["mute"].toBool());
		setAudioInputVolume(obj["volume"].toInt());
	}

	void setRtcEventCallback(RtcEventCallback cb)
	{
		m_rtccb = cb;
	}

	void sendEvent(int type, QJsonObject data)
	{
		if (!m_rtccb)
			return;

		m_rtccb(type, data);
	}

public slots:
	void onSpeakerEvent(const QJsonObject &data)
	{
		QJsonObject ret;
		bool self = data["self"].toBool();
		bool remote = data["remote"].toBool();
		if (self)
			ret["self"] = link_uid;
		if (remote)
			ret["other"] = link_otherUid;

		sendEvent(RTC_EVENT_USER_VOLUME, ret);
	}

private:
	QRect m_cropInfo = QRect(0, 0, 1920, 1080);
	VideoInfo m_vinfo;
	QJsonObject m_linkInfo;
	RtcEventCallback m_rtccb = nullptr;

public:
	QString link_rtc_uid;
	QString link_rtc_otherUid;
	QString link_uid;
	QString link_otherUid;
	QString link_rtcRoomId;
	QString link_streamUrl;
	QString link_streamId;
	QString link_cdnSupplier;
	QString link_rtcRoomToken;
	int link_rtcAPPID;
	int link_cdnAPPID;
	int link_cdnBizID;
	std::string link_std_rtcRoomId;
	std::string link_std_rtcUid;
	std::string link_std_rtcRoomToken;
	int link_type;
	bool is_video_link;
	QString last_audio_input_device;
};

class QNRtc;
class QINIURTC : public RTCBase {
	Q_OBJECT
public:
	QINIURTC();
	~QINIURTC();
	virtual void init();
	virtual void enterRoom();
	virtual void exitRoom();
	virtual void setRemoteViewHwnd(long window);
	virtual void sendAudio(struct audio_data *data);
	virtual void sendVideo(struct video_data *data);
	virtual void setSei(const QJsonObject &data, int insetType);
	virtual void setAudioInputDevice(const QString &deviceId);
	virtual void setAudioInputMute(bool mute);
	virtual void setAudioInputVolume(int volume);
	virtual void setAudioOutputDevice(const QString &deviceId);

private:
	QNRtc *m_rtc;
	bool m_joinSucess = false;
	bool m_subscibeSucess = false;
	bool m_publishSucess = false;
};

class TRTC : public RTCBase {
	Q_OBJECT
public:
	TRTC();
	~TRTC();
	virtual void init();
	virtual void enterRoom();
	virtual void exitRoom();
	virtual void setRemoteViewHwnd(long window);
	virtual void sendAudio(struct audio_data *data);
	virtual void sendVideo(struct video_data *data);
	virtual void setSei(const QJsonObject &data, int insetType);
	virtual void setAudioInputDevice(const QString &deviceId);
	virtual void setAudioInputMute(bool mute);
	virtual void setAudioInputVolume(int volume);
	virtual void setAudioOutputDevice(const QString &deviceId);

private:
	void internalEnterRoom();

	void onEnterRoom(int result);
	void onExitRoom();
	void onUserAudioAvailable(QString userId, bool available);
	void onUserVideoAvailable(QString userId, bool available);
	void onRemoteUserEnter(QString userId);
	void onRemoteUserLeave(QString userId);

private:
	bool m_bStartCustomCapture = false;
	long m_remoteView;
	char *m_yuvBuffer = nullptr;
	QEventLoop m_exitRoomLoop;
	QTimer m_seiTimer;
	QByteArray m_seiData;
	QByteArray m_uuid;
	bool m_hasMixStream = false;
};
