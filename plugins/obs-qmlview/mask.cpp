#include "mask.h"
#include "renderer.h"
#include <QDebug>

Mask::Mask(QObject *parent /* = nullptr */) : QmlSourceBase(parent)
	, m_status(-1)
	, m_toothIndex(-1)
	, m_sharkMaskXPos(0)
{
	addProperties("maskProperties", this);
}

void Mask::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file", "qrc:/qmlfiles/Mask.qml");
}

static const char *mask_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("maskshow");
}

static void mask_source_update(void *data, obs_data_t *settings)
{
	Mask *s = (Mask *)data;
	s->baseUpdate(settings);
}

static void *mask_source_create(obs_data_t *settings, obs_source_t *source)
{
	Mask *context = new Mask();
	context->setSource(source);

	mask_source_update(context, settings);
	return context;
}

static void mask_source_destroy(void *data)
{
	if (!data)
		return;
	Mask *s = (Mask *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void mask_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 1920);
	obs_data_set_default_int(settings, "height", 1080);
	Mask::default(settings);
}

static obs_properties_t *mask_source_properties(void *data)
{
	if (!data)
		return nullptr;
	Mask *s = (Mask *)data;
	obs_properties_t *props = s->baseProperties();

	obs_properties_add_text(props, "qmlpath", u8"qml源文件路径", OBS_TEXT_DEFAULT);
	return props;
}

static void maskCommand(void *data, obs_data_t *cmd)
{
	Mask *s = (Mask *)data;
	const char *cmdType = obs_data_get_string(cmd, "type");
	if (strcmp("sharkStaus", cmdType) == 0) {
		int status = obs_data_get_int(cmd, "status");
		s->setstatus(status);
		if (status == 1)
		{
			s->settoothIndex(obs_data_get_int(cmd, "toothIndex"));
		}
	}
	else if (strcmp("maskXPos", cmdType) == 0)
	{
		s->setsharkMaskXPos(obs_data_get_int(cmd, "pos"));
	}
}

struct obs_source_info mask_source_info = {
	"mask_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_DO_NOT_DUPLICATE,
	mask_source_get_name,
	mask_source_create,
	mask_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	mask_source_defaults,
	mask_source_properties,
	mask_source_update,
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
	nullptr,
	maskCommand};
