#pragma once

#include <QObject>
#include "qmlsourcebase.h"

class GiftTV : public QmlSourceBase {
	Q_OBJECT
public:
	enum PreferLayout { Horizontal = 0, Vertical };
	Q_ENUM(PreferLayout)

	DEFINE_PROPERTY(PreferLayout, prefer)
public:
	GiftTV(QObject *parent = nullptr);
	static void default(obs_data_t *settings);
};
