#pragma once

#include <QObject>
#include "qmlsourcebase.h"

class NewTimer : public QmlSourceBase {
	Q_OBJECT
public:
	DEFINE_PROPERTY(float, transparence)
	DEFINE_PROPERTY(long long, time)
	DEFINE_PROPERTY(int, themetype)
	DEFINE_PROPERTY(QString, themefont)
	DEFINE_PROPERTY(bool, themebold)
	DEFINE_PROPERTY(bool, themeitalic)
	DEFINE_PROPERTY(QString, themefontcolor)
	DEFINE_PROPERTY(int, timetype)
	DEFINE_PROPERTY(QString, text)
public:
	NewTimer(QObject *parent = nullptr);
	static void default(obs_data_t *settings);
signals:
	void replay();
	void update();
};
