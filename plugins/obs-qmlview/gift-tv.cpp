#include "gift-tv.h"
#include "renderer.h"
#include <QDebug>

GiftTV::GiftTV(QObject *parent /* = nullptr */) : QmlSourceBase(parent)
{
	addProperties("gifttvProperties", this);
}

void GiftTV::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
				    "qrc:/qmlfiles/GiftTV.qml");
}

static const char *gifttv_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("gifttv");
}

static void gifttv_source_update(void *data, obs_data_t *settings)
{
	GiftTV *s = (GiftTV *)data;
	s->baseUpdate(settings);
}

static void *gifttv_source_create(obs_data_t *settings, obs_source_t *source)
{
	GiftTV *context = new GiftTV();
	context->setSource(source);

	gifttv_source_update(context, settings);
	return context;
}

static void gifttv_source_destroy(void *data)
{
	if (!data)
		return;
	GiftTV *s = (GiftTV *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void gifttv_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 932);
	obs_data_set_default_int(settings, "height", 600);

	obs_data_set_default_int(settings, "triggerCondition", 1);
	obs_data_set_default_int(settings, "triggerConditionValue", 1000);
	obs_data_set_default_int(settings, "row", 5);
	obs_data_set_default_int(settings, "col", 2);
	obs_data_set_default_int(settings, "prefer", 0);
	obs_data_set_default_int(settings, "mode", 0);
	obs_data_set_default_int(settings, "disappear", 0);

	GiftTV::default(settings);
}

static obs_properties_t *gifttv_source_properties(void *data)
{
	if (!data)
		return nullptr;
	GiftTV *s = (GiftTV *)data;
	obs_properties_t *props = s->baseProperties();

	obs_properties_add_int(props, "triggerCondition", u8"触发条件", 0, 1,
			       1);
	obs_properties_add_int(props, "triggerConditionValue", u8"单价设定", 0,
			       999999, 1000);
	obs_properties_add_int(props, "row", u8"行", 1, 9,
			       1);
	obs_properties_add_int(props, "col", u8"列", 1, 4,
			       1);
	obs_properties_add_int(props, "prefer", u8"优先展示", 0, 2,
			       1);
	obs_properties_add_int(props, "mode", u8"展示模式", 0, 2,
			       1);
	obs_properties_add_int(props, "disappear", u8"消失设定", 0, 2,
			       1);
	return props;
}

struct obs_source_info gifttv_source_info = {
	"gifttv_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_DO_NOT_DUPLICATE,
	gifttv_source_get_name,
	gifttv_source_create,
	gifttv_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	gifttv_source_defaults,
	gifttv_source_properties,
	gifttv_source_update,
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
	nullptr};
