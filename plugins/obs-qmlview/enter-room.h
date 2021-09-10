#pragma once

#include <QObject>
#include "qmlsourcebase.h"

class EnterRoom : public QmlSourceBase {
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
		DEFINE_PROPERTY(int, uitype)
		DEFINE_PROPERTY(int, staytime)
		DEFINE_PROPERTY(bool, canstay)
public:
	EnterRoom(QObject *parent = nullptr);
	~EnterRoom();
	static void default(obs_data_t *settings);
	void setText(const QString &text);
	void doStart(bool isenter);
	void doHide();
	QString text();
signals:
	void replay();
	void update();
	void show();
	void hide();
	void enter();
private:
	int m_theme = 3;
	QString m_text;
	QTimer m_timer;
};
