#include "leave.h"
#include "renderer.h"
#include <QDebug>


Leave::Leave(QObject *parent /* = nullptr */)
	: QmlSourceBase(parent)
{
	addProperties("leaveProperties", this);
}

void Leave::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
				    "qrc:/qmlfiles/Leave.qml");
}

static const char *leave_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("quickleaveshow");
}

static void leave_source_update(void *data, obs_data_t *settings)
{
	Leave *s = (Leave *)data;
	s->baseUpdate(settings);

	const char *backgroundImage = obs_data_get_string(settings, "backgroundImage");
	s->setbackgroundImage(backgroundImage);

	const char *currentTime = obs_data_get_string(settings, "currentTime");
	s->setcurrentTime(currentTime);
}

static void *leave_source_create(obs_data_t *settings, obs_source_t *source)
{
	Leave *context = new Leave();
	context->setSource(source);

	leave_source_update(context, settings);
	return context;
}

static void leave_source_destroy(void *data)
{
	if (!data)
		return;
	Leave *s = (Leave *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void leave_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 1920);
	obs_data_set_default_int(settings, "height", 1080);
	Leave::default(settings);
}

static void leave_source_show(void *data)
{
	if (!data)
		return;
	Leave *s = (Leave *)data;
	s->baseShow();
}

static void leave_source_hide(void *data)
{
	if (!data)
		return;
	Leave *s = (Leave *)data;
	s->baseHide();
}

static uint32_t leave_source_getwidth(void *data)
{
	if (!data)
		return 5;
	Leave *s = (Leave *)data;
	return s->baseGetWidth();
}

static uint32_t leave_source_getheight(void *data)
{
	if (!data)
		return 5;
	Leave *s = (Leave *)data;
	return s->baseGetHeight();
}

static void leave_source_render(void *data, gs_effect_t *effect)
{
	if (!data)
		return;
	Leave *s = (Leave *)data;
	s->baseRender(effect);
}

static obs_properties_t *leave_source_properties(void *data)
{
	if (!data)
		return nullptr;
	Leave *s = (Leave *)data;
	obs_properties_t *props = s->baseProperties();

	obs_properties_add_text(props, "backgroundImage", u8"背景图", OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "currentTime", u8"当前计时", OBS_TEXT_DEFAULT);
	return props;
}

static void leave_source_mouse_click(void *data,
					 const struct obs_mouse_event *event,
					 int32_t type, bool mouse_up,
					 uint32_t click_count)
{
	if (!data)
		return;
	Leave *s = (Leave *)data;
	s->baseMouseClick(event->x, event->y, type, mouse_up, click_count);
}

static void leave_source_mouse_move(void *data,
					const struct obs_mouse_event *event,
					bool mouse_leave)
{
	if (!data)
		return;
	Leave *s = (Leave *)data;
	s->baseMouseMove(event->x, event->y, mouse_leave);
}

static void leave_source_mouse_wheel(void *data,
					 const struct obs_mouse_event *event,
					 int x_delta, int y_delta)
{
	if (!data)
		return;
	Leave *s = (Leave *)data;
	s->baseMouseWheel(x_delta, y_delta);
}

static void leave_source_focus(void *data, bool focus)
{
	if (!data)
		return;
	Leave *s = (Leave *)data;
	s->baseFocus(focus);
}

static void leave_source_key_click(void *data,
				       const struct obs_key_event *event,
				       bool key_up)
{
	if (!data)
		return;
	Leave *s = (Leave *)data;
	s->baseKey(event->native_scancode, event->native_vkey,
		   event->native_modifiers, event->text, key_up);
}

struct obs_source_info quickleave_source_info = {
	"quickleave_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_DO_NOT_DUPLICATE,
	leave_source_get_name,
	leave_source_create,
	leave_source_destroy,
	leave_source_getwidth,
	leave_source_getheight,
	leave_source_defaults,
	leave_source_properties,
	leave_source_update,
	nullptr,
	nullptr,
	leave_source_show,
	leave_source_hide,
	nullptr,
	leave_source_render,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	leave_source_mouse_click,
	leave_source_mouse_move,
	leave_source_mouse_wheel,
	leave_source_focus,
	leave_source_key_click,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr
};
