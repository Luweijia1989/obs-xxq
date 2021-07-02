#pragma once

#include <QObject>
#include "qmlsourcebase.h"

class NewFollow : public QmlSourceBase {
	Q_OBJECT
public:
	DEFINE_PROPERTY(float, transparence)
	DEFINE_PROPERTY(QString, firstname)
	DEFINE_PROPERTY(QString, avatarpath)
	DEFINE_PROPERTY(int, themetype)
	DEFINE_PROPERTY(QString, themefont)
	DEFINE_PROPERTY(bool, themebold)
	DEFINE_PROPERTY(bool, themeitalic)
	DEFINE_PROPERTY(QString, themefontcolor)
	DEFINE_PROPERTY(QString, datafont)
	DEFINE_PROPERTY(bool, databold)
	DEFINE_PROPERTY(bool, dataitalic)
	DEFINE_PROPERTY(QString, datafontcolor)
public:
	NewFollow(QObject *parent = nullptr);
	static void default(obs_data_t *settings);
signals:
	void replay();
	void update();
};
