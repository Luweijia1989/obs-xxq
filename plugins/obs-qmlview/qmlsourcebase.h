#pragma once

#include <QObject>
#include <QMutex>
#include "defines.h"
#include "qmlview.h"

class QmlSourceBase : public QObject {
	Q_OBJECT
public:
	QmlSourceBase(QObject *parent = nullptr);
	~QmlSourceBase();
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

void base_source_show(void *data);

void base_source_hide(void *data);

uint32_t base_source_getwidth(void *data);

uint32_t base_source_getheight(void *data);

void base_source_render(void *data, gs_effect_t *effect);

void base_source_mouse_click(void *data, const struct obs_mouse_event *event,
			     int32_t type, bool mouse_up, uint32_t click_count);

void base_source_mouse_move(void *data, const struct obs_mouse_event *event,
			    bool mouse_leave);
void base_source_mouse_wheel(void *data, const struct obs_mouse_event *event,
			     int x_delta, int y_delta);

void base_source_focus(void *data, bool focus);

void base_source_key_click(void *data, const struct obs_key_event *event,
			   bool key_up);
