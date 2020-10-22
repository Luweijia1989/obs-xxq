#pragma once

#include <QObject>
#include <QMutex>
#include "qmlsourcebase.h"

class ImageSlideShow : public QmlSourceBase {
	Q_OBJECT
public:
	enum SlideDirection {
		BottomToTop = 1,
		TopToBottom,
		RightToLeft,
		LeftToRight
	};
	Q_ENUM(SlideDirection)

	enum SlideSpeed { SpeedSlow = 1, SpeedMiddle, SpeedFast };
	Q_ENUM(SlideSpeed)

	DEFINE_PROPERTY(SlideDirection, direction)
	DEFINE_PROPERTY(SlideSpeed, speed)
	DEFINE_PROPERTY(QStringList, imageFiles)
	DEFINE_PROPERTY(Qt::Alignment, horizontalAlignment)
	DEFINE_PROPERTY(Qt::Alignment, verticalAlignment)
	DEFINE_PROPERTY(QColor, fillColor)

public:
	ImageSlideShow(QObject *parent = nullptr);
	static void default(obs_data_t *settings);
signals:
	void Replay();

public:
	QMutex m_mutex;
};
