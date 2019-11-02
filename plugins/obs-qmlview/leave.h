#pragma once

#include <QObject>
#include "qmlsourcebase.h"

class Leave : public QmlSourceBase {
	Q_OBJECT
public:
	DEFINE_PROPERTY(QString, backgroundImage)
	DEFINE_PROPERTY(QString, currentTime)

public:
	Leave(QObject *parent = nullptr);
	static void default(obs_data_t *settings);

public:
};
