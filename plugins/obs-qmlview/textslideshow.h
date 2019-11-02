#pragma once

#include <QObject>
#include <QMutex>
#include "qmlsourcebase.h"

class TextSlideShow : public QmlSourceBase {
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
	DEFINE_PROPERTY(QStringList, textStrings)
	DEFINE_PROPERTY(Qt::Alignment, horizontalAlignment)
	DEFINE_PROPERTY(Qt::Alignment, verticalAlignment)
	DEFINE_PROPERTY(QColor, fillColor)
	DEFINE_PROPERTY(QColor, frontColor)
	DEFINE_PROPERTY(bool, bold)
	DEFINE_PROPERTY(bool, italic)
	DEFINE_PROPERTY(bool, strikeout)
	DEFINE_PROPERTY(bool, underline)
	DEFINE_PROPERTY(QString, font)
	DEFINE_PROPERTY(int, fontSize)
	DEFINE_PROPERTY(bool, hDisplay)

public:
	TextSlideShow(QObject *parent = nullptr);
	static void default(obs_data_t *settings);

public:
	QMutex m_mutex;
};
