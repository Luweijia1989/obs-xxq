#pragma once

#include <QObject>
#include <QMutex>
#include "defines.h"
#include "qmlview.h"

class QmlSourceBase : public QObject {
	Q_OBJECT
public:
	QmlSourceBase(QObject *parent = nullptr);
	void addProperties(QString name, QObject *value);
	void setSource(obs_source_t *source);
	void baseUpdate(obs_data_t *settings);
	void baseDestroy();
	static void baseDefault(obs_data_t *settings);
	void baseShow();
	void baseHide();
	uint32_t baseGetWidth();
	uint32_t baseGetHeight();
	void baseRender(gs_effect_t *effect);
	void baseTick(float seconds);
	obs_properties_t *baseProperties();
	void baseMouseClick(quint32 x, quint32 y, qint32 type, bool onoff,
			    quint8 click_count);
	void baseMouseMove(quint32 x, quint32 y, bool leaving);
	void baseMouseWheel(qint32 xdelta, qint32 ydelta);
	void baseKey(quint32 keycode, quint32 vkey, quint32 modifiers,
		     const char *text, bool updown);
	void baseFocus(bool onoff);

public:
	OBSQuickview *quickView;
};
