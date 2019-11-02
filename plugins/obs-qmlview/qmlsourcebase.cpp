#include "qmlsourcebase.h"
#include "renderer.h"

QmlSourceBase::QmlSourceBase(QObject *parent /*= nullptr*/) : QObject(parent)
{
	quickView = new OBSQuickview(this);
}

void QmlSourceBase::addProperties(QString name, QObject *value)
{
	quickView->m_quickView->addContextProperties(name, value);
}

void QmlSourceBase::setSource(obs_source_t *source)
{
	quickView->source = source;
}

void QmlSourceBase::baseUpdate(obs_data_t *settings)
{
	const char *file = obs_data_get_string(settings, "file");
	const bool unload = obs_data_get_bool(settings, "unload");
	const bool force = obs_data_get_bool(settings, "force");
	quint32 w = obs_data_get_int(settings, "width");
	quint32 h = obs_data_get_int(settings, "height");
	quint32 fps = obs_data_get_int(settings, "fps");
	quickView->m_fps = fps;
	quickView->m_frameLimited = (fps > 0);

	quickView->m_quickView->manualUpdated(fps > 0);
	// s->m_quickView->manualUpdated(force);

	if (w > 10 && h > 10)
		quickView->resize(w, h);

	quickView->m_persistent = !unload;
	QString str(file);
	if (str != quickView->m_last_url) {
		QUrl url(str);
		if (url.isValid() && !url.isEmpty()) {
			if (url.isLocalFile()) {
				QFileInfo fi(url.toLocalFile());
				if (fi.exists())
					quickView->loadUrl(url);
			} else
				quickView->loadUrl(url);
		}
		quickView->m_last_url = str;
	}
}

void QmlSourceBase::baseDestroy()
{
	quickView->m_delete = true;
}

void QmlSourceBase::baseDefault(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "unload", false);
	obs_data_set_default_int(settings, "width", 640);
	obs_data_set_default_int(settings, "height", 480);
}

void QmlSourceBase::baseShow()
{
	quickView->obsshow();
}

void QmlSourceBase::baseHide()
{
	quickView->obshide();
}

uint32_t QmlSourceBase::baseGetWidth()
{
	return quickView->width();
}

uint32_t QmlSourceBase::baseGetHeight()
{
	return quickView->height();
}

void QmlSourceBase::baseRender(gs_effect_t *effect)
{
	quickView->renderFrame(effect);
}

void QmlSourceBase::baseTick(float seconds)
{
	quickView->tick(seconds);
}

obs_properties_t *QmlSourceBase::baseProperties()
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_text(
		props, "file",
		obs_module_text("URL (eg: file:///C:/scenes/main.qml)"),
		OBS_TEXT_DEFAULT);
	obs_properties_add_bool(props, "unload",
				obs_module_text("Reload when made visible"));
	obs_properties_add_bool(props, "force",
				obs_module_text("Force rendering"));
	obs_properties_add_int(props, "fps",
			       obs_module_text("Limited FPS (0 for unlimited"),
			       0, 60, 1);
	obs_properties_add_int(props, "width", obs_module_text("Width"), 10,
			       4096, 1);
	obs_properties_add_int(props, "height", obs_module_text("Height"), 10,
			       4096, 1);

	return props;
}

void QmlSourceBase::baseMouseClick(quint32 x, quint32 y, qint32 type,
				   bool onoff, quint8 click_count)
{
	quickView->m_quickView->sendMouseClick(x, y, type, onoff, click_count);
}

void QmlSourceBase::baseMouseMove(quint32 x, quint32 y, bool leaving)
{
	quickView->m_quickView->sendMouseMove(x, y, leaving);
}

void QmlSourceBase::baseMouseWheel(qint32 xdelta, qint32 ydelta)
{
	quickView->m_quickView->sendMouseWheel(xdelta, ydelta);
}

void QmlSourceBase::baseKey(quint32 keycode, quint32 vkey, quint32 modifiers,
			    const char *text, bool updown)
{
	quickView->m_quickView->sendKey(keycode, vkey, modifiers, text, updown);
}

void QmlSourceBase::baseFocus(bool onoff)
{
	quickView->m_quickView->sendFocus(onoff);
}
