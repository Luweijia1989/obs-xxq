#pragma once

#include <QObject>
#include <QMutex>
#include "qmlsourcebase.h"

class FansTarget : public QmlSourceBase {
	Q_OBJECT
public:
	DEFINE_PROPERTY(float, transparence)
	DEFINE_PROPERTY(int, totalfans)
	DEFINE_PROPERTY(int, realfans)
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
	FansTarget(QObject *parent = nullptr);
	static void default(obs_data_t *settings);

public:
	QMutex m_mutex;
};
