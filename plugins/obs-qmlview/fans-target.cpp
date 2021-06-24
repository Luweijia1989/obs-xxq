#include "fans-target.h"
#include "renderer.h"
#include "qmlhelper.h"

//QML_REGISTER_CREATABLE_TYPE(TextSlideShow, TextSlideShow)

FansTarget::FansTarget(QObject *parent /* = nullptr */) : QmlSourceBase(parent)
{
	addProperties("fansTargetProperties", this);
}

void FansTarget::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
				    "qrc:/qmlfiles/FansTarget.qml");
	obs_data_set_default_string(settings, "themefont",
				    u8"阿里巴巴普惠体 M");
	obs_data_set_default_bool(settings, "themebold", false);
	obs_data_set_default_bool(settings, "themeitalic", false);

	obs_data_set_default_string(settings, "datafont", u8"阿里巴巴普惠体 R");
	obs_data_set_default_bool(settings, "databold", false);
	obs_data_set_default_bool(settings, "dataitalic", false);

	obs_data_set_default_int(settings, "themetype", 1);
	obs_data_set_default_int(settings, "realfans", 0);
	obs_data_set_default_int(settings, "totalfans", 0);

	obs_data_set_default_string(settings, "themefontcolor", "#333333");
	obs_data_set_default_string(settings, "datafontcolor", "#FFFFFF");
	obs_data_set_default_int(settings, "transparence", 100);
}

static const char *fanstarget_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("quicktargetfansshow");
}

static void fanstarget_source_update(void *data, obs_data_t *settings)
{
	FansTarget *s = (FansTarget *)data;
	s->baseUpdate(settings);

	bool bold1 = obs_data_get_bool(settings, "databold");
	s->setdatabold(bold1);

	bool italic1 = obs_data_get_bool(settings, "dataitalic");
	s->setdataitalic(italic1);

	const char *font1 = obs_data_get_string(settings, "datafont");
	s->setdatafont(font1);

	const char *datafontcolor =
		obs_data_get_string(settings, "datafontcolor");
	s->setdatafontcolor(datafontcolor);

	bool bold2 = obs_data_get_bool(settings, "themebold");
	s->setthemebold(bold2);

	bool italic2 = obs_data_get_bool(settings, "themeitalic");
	s->setthemeitalic(italic2);

	const char *font2 = obs_data_get_string(settings, "themefont");
	s->setthemefont(font2);

	const char *themefontcolor =
		obs_data_get_string(settings, "themefontcolor");
	s->setthemefontcolor(themefontcolor);

	int themeType = obs_data_get_int(settings, "themetype");
	s->setthemetype(themeType);

	int totalfans = obs_data_get_int(settings, "totalfans");
	s->settotalfans(totalfans);

	int realfans = obs_data_get_int(settings, "realfans");
	s->setrealfans(realfans);

	float transparence = (float)obs_data_get_int(settings, "transparence");
	transparence = transparence / 100.0f;
	s->settransparence(transparence);
}

static void *fanstarget_source_create(obs_data_t *settings,
				      obs_source_t *source)
{
	FansTarget *context = new FansTarget();
	context->setSource(source);

	fanstarget_source_update(context, settings);
	return context;
}

static void fanstarget_source_destroy(void *data)
{
	if (!data)
		return;
	FansTarget *s = (FansTarget *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void fanstarget_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 250);
	obs_data_set_default_int(settings, "height", 80);
	FansTarget::default(settings);
}

static obs_properties_t *fanstarget_source_properties(void *data)
{
	if (!data)
		return nullptr;
	FansTarget *s = (FansTarget *)data;
	obs_properties_t *props = s->baseProperties();
	return props;
}

struct obs_source_info quickfanstargetshow_source_info = {
	"quickfanstargetshow_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION,
	fanstarget_source_get_name,
	fanstarget_source_create,
	fanstarget_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	fanstarget_source_defaults,
	fanstarget_source_properties,
	fanstarget_source_update,
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
