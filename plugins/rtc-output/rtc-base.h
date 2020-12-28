#pragma once

#include <obs-module.h>
#include <QObject>
#include <QJsonObject>

class RTCBase : public QObject {
	Q_OBJECT
public:
	virtual void init() = 0;
	virtual void enterRoom() = 0;
	virtual void switchRoom(int id) = 0;

private:
	virtual void onEvent(int type, QJsonObject data){};
};

class QINIURTC : public RTCBase {
public:
	QINIURTC();
	virtual void init();
	virtual void enterRoom();
	virtual void switchRoom(int id);
};

class TRTC : public RTCBase {
	Q_OBJECT
public:
	TRTC();
	virtual void init();
	virtual void enterRoom();
	virtual void switchRoom(int id);

private:
	void onEnterRoom(int result);
	void onExitRoom();
	void onUserAudioAvailable(QString userId, bool available);
	void onUserVideoAvailable(QString userId, bool available);
	void onSwitchRoom(int errCode, QString errMsg);

signals:
	void onEvent(int type, QJsonObject data) Q_DECL_OVERRIDE;
};
