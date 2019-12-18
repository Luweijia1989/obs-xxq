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

struct obs_source_info quickleave_source_info = {
	"quickleave_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_DO_NOT_DUPLICATE,
	leave_source_get_name,
	leave_source_create,
	leave_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	leave_source_defaults,
	leave_source_properties,
	leave_source_update,
	nullptr,
	nullptr,
	base_source_show,
	base_source_hide,
	nullptr,
	base_source_render,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	base_source_mouse_click,
	base_source_mouse_move,
	base_source_mouse_wheel,
	base_source_focus,
	base_source_key_click,
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
