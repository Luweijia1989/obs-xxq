#pragma once

#include <QObject>
#include "qmlsourcebase.h"

class AudioLiveLink : public QmlSourceBase {
	Q_OBJECT
	DEFINE_PROPERTY(QString, path)
	DEFINE_PROPERTY(QString, name)
	DEFINE_PROPERTY(QString, wave)
	DEFINE_PROPERTY(bool, isMuliti)
	DEFINE_PROPERTY(QString, effect)
	DEFINE_PROPERTY(int, multiCount)
	DEFINE_PROPERTY(int, posX)
	DEFINE_PROPERTY(int, posY)

	// 连麦相关的配置
	DEFINE_PROPERTY(int, voiceWaveLeftMargin)
	DEFINE_PROPERTY(int, voiceWaveTopMargin)
	DEFINE_PROPERTY(int, voiceWaveSize)
	DEFINE_PROPERTY(int, avatarPos)
	DEFINE_PROPERTY(int, avatarSize)
	DEFINE_PROPERTY(int, borderWidth)
	DEFINE_PROPERTY(int, backWidth)
	DEFINE_PROPERTY(int, basicWidth)
	DEFINE_PROPERTY(int, basicHeight)
	DEFINE_PROPERTY(float, scale)
public:
	AudioLiveLink(QObject *parent = nullptr);
	static void default(obs_data_t *settings);
signals:
	void play();
	void stop();
	void showPkEffect();
	void stopPkEffect();
	void link();
};
