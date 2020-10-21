#pragma once

#include <QObject>
#include "qmlsourcebase.h"

class GiftTV : public QmlSourceBase {
	Q_OBJECT
public:
	
public:
	GiftTV(QObject *parent = nullptr);
	static void default(obs_data_t *settings);
};
