#pragma once

#include <obs-module.h>
#include <QObject>
#include <QJsonObject>
#include <QRect>

class RTCBase : public QObject {
	Q_OBJECT
public:
	virtual void init() = 0;
	virtual void enterRoom(QString userId, int roomId, QString userSig) = 0;
	virtual void exitRoom() = 0;
	virtual void switchRoom(int roomId) = 0;
	virtual void connectOtherRoom(QString userId, int roomId) = 0;
	virtual void disconnectOtherRoom() = 0;
	virtual void setRemoteViewHwnd(long window) = 0;
	virtual void sendAudio(struct audio_data *data) = 0;
	virtual void sendVideo(struct video_data *data) = 0;
	void setCropInfo(int x)
	{
		if (x + 720 > 1920)
			m_cropInfo = QRect(0, 0, 720, 1080);
		else
			m_cropInfo = QRect(x, 0, 720, 1080);
	}

	const QRect &cropInfo()
	{
		return m_cropInfo;
	}
private:
	virtual void onEvent(int type, QJsonObject data){};
	QRect m_cropInfo = QRect(0, 0, 1920, 1080);
};

class QINIURTC : public RTCBase {
	Q_OBJECT
public:
	QINIURTC();
	virtual void init();
	virtual void enterRoom(QString userId, int roomId, QString userSig);
	virtual void exitRoom();
	virtual void switchRoom(int roomId);
	virtual void connectOtherRoom(QString userId, int roomId);
	virtual void disconnectOtherRoom();
	virtual void setRemoteViewHwnd(long window);
	virtual void sendAudio(struct audio_data *data);
	virtual void sendVideo(struct video_data *data);
};

class TRTC : public RTCBase {
	Q_OBJECT
public:
	TRTC();
	~TRTC();
	virtual void init();
	virtual void enterRoom(QString userId, int roomId, QString userSig);
	virtual void exitRoom();
	virtual void switchRoom(int roomId);
	virtual void connectOtherRoom(QString userId, int roomId);
	virtual void disconnectOtherRoom();
	virtual void setRemoteViewHwnd(long window);
	virtual void sendAudio(struct audio_data *data);
	virtual void sendVideo(struct video_data *data);

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

signals:
	void onEvent(int type, QJsonObject data) Q_DECL_OVERRIDE;

private:
	bool m_bStartCustomCapture = false;
	int m_pkRoomId;
	long m_remoteView;
	char *m_yuvBuffer = nullptr;
};
