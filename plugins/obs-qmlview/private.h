#pragma once

#include <QObject>
#include "qmlsourcebase.h"

class Private : public QmlSourceBase {
	Q_OBJECT
public:
	Private(QObject *parent = nullptr);
	static void default(obs_data_t *settings);
};
