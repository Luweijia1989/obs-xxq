#pragma once

#include <QObject>
#include "qmlsourcebase.h"

class Mask : public QmlSourceBase {
	Q_OBJECT
	DEFINE_PROPERTY(int, status)
	DEFINE_PROPERTY(int, toothIndex)
	DEFINE_PROPERTY(int, sharkMaskXPos)
public:
	Mask(QObject *parent = nullptr);
	static void default(obs_data_t *settings);
};
