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
