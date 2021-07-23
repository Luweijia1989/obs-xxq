#pragma once

#include <QObject>
#include "qmlsourcebase.h"
#include <QTimer>
#include <QPointer>

class FirstRankList : public QmlSourceBase {
	Q_OBJECT
public:
	DEFINE_PROPERTY(float, transparence)
	DEFINE_PROPERTY(int, listtype)
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
	DEFINE_PROPERTY(int, uitype)
public:
	FirstRankList(QObject *parent = nullptr);
	static void default(obs_data_t *settings);
	void setText(const QString &text);
	QString text();
signals:
	void replay();
	void update();

private:
	QString m_text;
};
