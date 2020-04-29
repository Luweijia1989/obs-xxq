#include "private.h"
#include "renderer.h"
#include <QDebug>


Private::Private(QObject *parent /* = nullptr */)
	: QmlSourceBase(parent)
{
	addProperties("privateProperties", this);
}

void Private::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file", "qrc:/qmlfiles/Private.qml");
}

static const char *private_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("quickprivateshow");
}

static void private_source_update(void *data, obs_data_t *settings)
{
	Private *s = (Private *)data;
	s->baseUpdate(settings);
}

static void *private_source_create(obs_data_t *settings, obs_source_t *source)
{
	Private *context = new Private();
	context->setSource(source);

	private_source_update(context, settings);
	return context;
}

static void private_source_destroy(void *data)
{
	if (!data)
		return;
	Private *s = (Private *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void private_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 1920);
	obs_data_set_default_int(settings, "height", 1080);
	Private::default(settings);
}

static obs_properties_t *private_source_properties(void *data)
{
	if (!data)
		return nullptr;
	Private *s = (Private *)data;
	obs_properties_t *props = s->baseProperties();
	return props;
}

struct obs_source_info quickprivate_source_info = {
	"quickprivate_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_DO_NOT_DUPLICATE,
	private_source_get_name,
	private_source_create,
	private_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	private_source_defaults,
	private_source_properties,
	private_source_update,
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
