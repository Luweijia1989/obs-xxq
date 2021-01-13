#pragma once

#include <obs-module.h>
#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRect>
#include <QEventLoop>

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
	virtual void switchRoom(int roomId) = 0;
	virtual void connectOtherRoom(QString userId, int roomId) = 0;
	virtual void disconnectOtherRoom() = 0;
	virtual void mixStream(QJsonObject param) = 0;
	virtual void setRemoteViewHwnd(long window) = 0;
	virtual void sendAudio(struct audio_data *data) = 0;
	virtual void sendVideo(struct video_data *data) = 0;
	virtual void setSei(const QJsonObject &data, int insetType) = 0;
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
		m_linkInfo = jd.object();
	}

	const QJsonObject &linkInfo() { return m_linkInfo; }

signals:
	void onEvent(int type, QJsonObject data);

private:
	QRect m_cropInfo = QRect(0, 0, 1920, 1080);
	VideoInfo m_vinfo;
	QJsonObject m_linkInfo;
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
	virtual void switchRoom(int roomId);
	virtual void connectOtherRoom(QString userId, int roomId);
	virtual void disconnectOtherRoom();
	virtual void mixStream(QJsonObject param);
	virtual void setRemoteViewHwnd(long window);
	virtual void sendAudio(struct audio_data *data);
	virtual void sendVideo(struct video_data *data);
	virtual void setSei(const QJsonObject &data, int insetType);

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
	virtual void switchRoom(int roomId);
	virtual void connectOtherRoom(QString userId, int roomId);
	virtual void disconnectOtherRoom();
	virtual void mixStream(QJsonObject param);
	virtual void setRemoteViewHwnd(long window);
	virtual void sendAudio(struct audio_data *data);
	virtual void sendVideo(struct video_data *data);
	virtual void setSei(const QJsonObject &data, int insetType);

private:
	void internalEnterRoom();

	void onEnterRoom(int result);
	void onExitRoom();
	void onUserAudioAvailable(QString userId, bool available);
	void onUserVideoAvailable(QString userId, bool available);
	void onSwitchRoom(int errCode, QString errMsg);
	void onRemoteUserEnter(QString userId);
	void onRemoteUserLeave(QString userId);
	void onConnectOtherRoom(QString userId, int errCode, QString errMsg);
	void onDisconnectOtherRoom(int errCode, QString errMsg);

private:
	bool m_bStartCustomCapture = false;
	bool m_bSendMixRequest = false;
	std::string m_pkRoomId;
	long m_remoteView;
	char *m_yuvBuffer = nullptr;
	QEventLoop m_exitRoomLoop;
};
