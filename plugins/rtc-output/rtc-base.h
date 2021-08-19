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
#include "trtc/TRTCTypeDef.h"

typedef std::function< void(int type, QJsonObject data) > RtcEventCallback;

class RTCBase : public QObject {
	Q_OBJECT
public:
	struct RtcEnterInfo {
		bool videoOnly;
		int appId;
		int roomId;
		QString uid;
		QString userId;
		QString userSig;
	};

	struct VideoEncodeInfo {
		int canvasWidth;
		int canvasHeight;
		int outputWidth;
		int outputHeight;
		int width;
		int height;
		int fps;
		int bitrate;
	};

	struct RoomUser {
		bool isSelf = false;
		QString uid;
		QString userId;
		int renderView;
		int roomId;
		std::string stdUserId;
		std::string stdRoomId;
		bool isAnchor;
		bool isCross;
		bool audioAvailable = false;
		bool mute = false;
	};

	struct CloudMixInfo {
		int cdnAppId;
		int cdnBizId;
		int mixWidth;
		int mixHeight;
		int mixVideoBitRate;
		int mixFps;
		int mixUsers;
		bool onlyMixAnchorVideo;
		bool usePresetLayout;
		QString cdnSupplier;
		QString streamUrl;
		QString streamId;
	};

	virtual void init() = 0;
	virtual void enterRoom() = 0;
	virtual void exitRoom() = 0;
	virtual void sendAudio(struct audio_data *data) = 0;
	virtual void sendVideo(struct video_data *data) = 0;
	virtual void setSei(const QJsonObject &data, int insetType) = 0;
	virtual void setAudioInputDevice(const QString &deviceId) = 0;
	virtual void setAudioInputMute(bool mute) = 0;
	virtual void setAudioInputVolume(int volume) = 0;
	virtual void setAudioOutputDevice(const QString &deviceId) = 0;
	virtual void setAudioOutputMute(bool mute) = 0;
	virtual void setAudioOutputVolume(int volume) = 0;
	virtual void stopTimeoutTimer() = 0;
	virtual uint64_t getTotalBytes() = 0;
	virtual void startRecord(const QString &path) = 0;
	virtual void stopRecord() = 0;
	virtual void connectOtherRoom(const QString &userId, int roomId, const QString &uid, bool selfDoConnect) = 0;
	virtual void disconnectOtherRoom() = 0;
	virtual void muteRemoteAnchor(bool mute) = 0;
	void setCropInfo(int x, int cropWidth)
	{
		m_cropInfo = QRect(x, 0, cropWidth, videoEncodeInfo.outputHeight);
	}

	const QRect &cropInfo() { return m_cropInfo; }

	

	void setRtcEnterInfo(const char *str)
	{
		QJsonDocument jd = QJsonDocument::fromJson(str);
		QJsonObject data = jd.object();
		rtcEnterInfo.videoOnly = data["videoOnly"].toBool();
		rtcEnterInfo.appId = data["appId"].toString().toInt();
		rtcEnterInfo.roomId = data["roomId"].toString().toInt();
		rtcEnterInfo.uid = data["uid"].toString();
		rtcEnterInfo.userId = data["userId"].toString();
		rtcEnterInfo.userSig = data["userSig"].toString();
	}

	void setRemoteUserInfos(const char *str)
	{
		QJsonDocument jd = QJsonDocument::fromJson(str);
		QJsonArray data = jd.array();
		for (int i=0; i<data.size(); i++)
		{
			auto one = data.at(i).toObject();
			QString uid = one["uid"].toString();
			QString userId = one["userId"].toString();

			m_roomUsers.insert(userId, {false, uid, userId, one["renderView"].toInt(-1), rtcEnterInfo.roomId, userId.toStdString(), QString::number(rtcEnterInfo.roomId).toStdString(), false, false, false, false});
		}
	}

	void setVideoEncodeInfo(const char *str)
	{
		QJsonDocument jd = QJsonDocument::fromJson(str);
		QJsonObject data = jd.object();
		videoEncodeInfo.canvasWidth = data["canvasWidth"].toInt();
		videoEncodeInfo.canvasHeight = data["canvasHeight"].toInt();
		videoEncodeInfo.width = data["width"].toInt();
		videoEncodeInfo.height = data["height"].toInt();
		videoEncodeInfo.fps = data["fps"].toInt();
		videoEncodeInfo.bitrate = data["bitrate"].toInt();
		videoEncodeInfo.outputWidth = data["outputWidth"].toInt();
		videoEncodeInfo.outputHeight = data["outputHeight"].toInt();
	}

	void setMixInfo(const char *str)
	{
		QJsonDocument jd = QJsonDocument::fromJson(str);
		QJsonObject data = jd.object();
		cloudMixInfo.cdnAppId = data["cdnAppId"].toString().toInt();
		cloudMixInfo.cdnBizId = data["cdnBizId"].toString().toInt();
		cloudMixInfo.mixWidth = data["mixWidth"].toInt();
		cloudMixInfo.mixHeight = data["mixHeight"].toInt();
		cloudMixInfo.mixVideoBitRate = data["mixVideoBitRate"].toInt();
		cloudMixInfo.mixFps = data["mixFps"].toInt();
		cloudMixInfo.mixUsers = data["mixUsers"].toInt();
		cloudMixInfo.onlyMixAnchorVideo = data["onlyMixAnchorVideo"].toBool(true);
		cloudMixInfo.cdnSupplier = data["cdnSupplier"].toString();
		cloudMixInfo.streamUrl = data["streamUrl"].toString();
		cloudMixInfo.streamId = data["streamId"].toString();
		cloudMixInfo.usePresetLayout = data["usePresetLayout"].toBool();
	}

	void setMicInfo(QString str)
	{
		QJsonDocument jd = QJsonDocument::fromJson(str.toUtf8());
		QJsonObject obj = jd.object();
		setAudioInputVolume(obj["volume"].toInt());
		setAudioInputDevice(obj["device"].toString());
		setAudioInputMute(obj["mute"].toBool());
	}

	void setDesktopAudioInfo(QString str)
	{
		QJsonDocument jd = QJsonDocument::fromJson(str.toUtf8());
		QJsonObject obj = jd.object();
		setAudioOutputVolume(obj["volume"].toInt());
		setAudioOutputDevice(obj["device"].toString());
		setAudioOutputMute(obj["mute"].toBool());
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

		if (type == RTC_EVENT_SUCCESS || type == RTC_EVENT_FAIL)
			stopTimeoutTimer();
	}

public slots:
	void onSpeakerEvent(const QJsonObject &data)
	{
		QJsonObject ret;
		QJsonArray users = data["users"].toArray();
		QJsonArray retArray;

		for (int i=0; i<users.count(); i++)
		{
			QString userid = users.at(i).toString();
			if (userid.isEmpty())
				retArray.append(rtcEnterInfo.uid);
			else if (m_roomUsers.contains(userid))
				retArray.append(m_roomUsers.value(userid).uid);
		}

		ret["speakers"] = retArray;
		sendEvent(RTC_EVENT_USER_VOLUME, ret);
	}

private:
	QRect m_cropInfo = QRect(0, 0, 1280, 720);
	QJsonObject m_linkInfo;
	RtcEventCallback m_rtccb = nullptr;

public:
	RtcEnterInfo rtcEnterInfo;
	VideoEncodeInfo videoEncodeInfo;
	CloudMixInfo cloudMixInfo;
	QMap<QString, RoomUser> m_roomUsers;
	int m_rtcCrossRoomId = -1;

	QString last_audio_input_device;
};

class TRTC : public RTCBase {
	Q_OBJECT
public:
	TRTC();
	~TRTC();
	virtual void init();
	virtual void enterRoom();
	virtual void exitRoom();
	virtual void sendAudio(struct audio_data *data);
	virtual void sendVideo(struct video_data *data);
	virtual void setSei(const QJsonObject &data, int insetType);
	virtual void setAudioInputDevice(const QString &deviceId);
	virtual void setAudioInputMute(bool mute);
	virtual void setAudioInputVolume(int volume);
	virtual void setAudioOutputDevice(const QString &deviceId);
	virtual void setAudioOutputMute(bool mute);
	virtual void setAudioOutputVolume(int volume);
	virtual void stopTimeoutTimer();
	virtual uint64_t getTotalBytes();
	virtual void startRecord(const QString &path);
	virtual void stopRecord();
	virtual void connectOtherRoom(const QString &userId, int roomId, const QString &uid, bool selfDoConnect);
	virtual void disconnectOtherRoom();
	virtual void muteRemoteAnchor(bool mute);

private:
	void internalEnterRoom();

	void onEnterRoom(int result);
	void onExitRoom();
	void onUserAudioAvailable(QString userId, bool available);
	void onUserVideoAvailable(QString userId, bool available);
	void onRemoteUserEnter(QString userId);
	void onRemoteUserLeave(QString userId);
	void updateRoomUsers();

private:
	bool m_bStartCustomCapture = false;
	char *m_yuvBuffer = nullptr;
	QEventLoop m_exitRoomLoop;
	QByteArray m_seiData;
	QByteArray m_uuid;
	bool m_hasMixStream = false;
	QTimer m_linkTimeout;

	bool m_bIsEnteredRoom = false;
	TRTCNetworkQosParam m_qosParams;
	TRTCAppScene m_sceneParams = TRTCAppSceneLIVE;
	TRTCRoleType m_roleType = TRTCRoleAnchor;

	int m_cacheAudioInputVolume = 0;
	int m_cacheAudioOutputVolume = 0;
};
