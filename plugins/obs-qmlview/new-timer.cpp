#include "new-timer.h"
#include "renderer.h"
#include "qmlhelper.h"

//QML_REGISTER_CREATABLE_TYPE(TextSlideShow, TextSlideShow)

NewTimer::NewTimer(QObject *parent /* = nullptr */) : QmlSourceBase(parent)
{
	addProperties("newTimerProperties", this);
}

void NewTimer::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
				    "qrc:/qmlfiles/NewTimer.qml");
	obs_data_set_default_string(settings, "themefont",
				    u8"阿里巴巴普惠体 M");
	obs_data_set_default_bool(settings, "themebold", false);
	obs_data_set_default_bool(settings, "themeitalic", false);
	obs_data_set_default_int(settings, "themetype", 1);
	obs_data_set_default_string(settings, "themefontcolor", "#FFFFFF");
	obs_data_set_default_int(settings, "transparence", 100);
	obs_data_set_default_int(settings, "timetype", 1);
	obs_data_set_default_int(settings, "time", 0);
}

static const char *newtimer_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("quicknewtimershow");
}

static void newtimer_source_update(void *data, obs_data_t *settings)
{
	bool textChange = false;
	NewTimer *s = (NewTimer *)data;
	s->baseUpdate(settings);

	bool bold2 = obs_data_get_bool(settings, "themebold");
	s->setthemebold(bold2);

	bool italic2 = obs_data_get_bool(settings, "themeitalic");
	s->setthemeitalic(italic2);

	const char *font2 = obs_data_get_string(settings, "themefont");
	s->setthemefont(font2);

	const char *themefontcolor =
		obs_data_get_string(settings, "themefontcolor");
	s->setthemefontcolor(themefontcolor);

	long long time = obs_data_get_int(settings, "time");
	s->settime(time);

	int themeType = obs_data_get_int(settings, "themetype");
	s->setthemetype(themeType);

	int timeType = obs_data_get_int(settings, "timetype");
	s->settimetype(timeType);

	float transparence = (float)(qMax((int)obs_data_get_int(settings, "transparence"), 1));
	transparence = transparence / 100.0f;
	s->settransparence(transparence);

	const char *text = obs_data_get_string(settings, "text");
	s->settext(text);
	//if (textChange)
	//emit s->update();
}

static void *newtimer_source_create(obs_data_t *settings, obs_source_t *source)
{
	NewTimer *context = new NewTimer();
	context->setSource(source);

	newtimer_source_update(context, settings);
	return context;
}

static void newtimer_source_destroy(void *data)
{
	if (!data)
		return;
	NewTimer *s = (NewTimer *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void newtimer_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 602);
	obs_data_set_default_int(settings, "height", 164);
	NewTimer::default(settings);
}

static obs_properties_t *newtimer_source_properties(void *data)
{
	if (!data)
		return nullptr;
	NewTimer *s = (NewTimer *)data;
	obs_properties_t *props = s->baseProperties();
	return props;
}

static void quickview_newtimersource_make_custom(void *data,
						 obs_data_t *command)
{
	if (!data)
		return;

	const char *cmdType = obs_data_get_string(command, "type");
	if (strcmp("replay", cmdType) == 0) {
		NewTimer *s1 = (NewTimer *)data;
		emit s1->replay();
	} else if (strcmp("update", cmdType) == 0) {
		NewTimer *s2 = (NewTimer *)data;
		emit s2->update();
	}
}

struct obs_source_info quicknewtimershow_source_info = {
	"quicknewtimershow_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION,
	newtimer_source_get_name,
	newtimer_source_create,
	newtimer_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	newtimer_source_defaults,
	newtimer_source_properties,
	newtimer_source_update,
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
	quickview_newtimersource_make_custom};
