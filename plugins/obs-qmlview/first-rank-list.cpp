#include "first-rank-list.h"
#include "renderer.h"
#include "qmlhelper.h"

//QML_REGISTER_CREATABLE_TYPE(TextSlideShow, TextSlideShow)

FirstRankList::FirstRankList(QObject *parent /* = nullptr */)
	: QmlSourceBase(parent)
{
	addProperties("firstRankListProperties", this);
}

void FirstRankList::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
				    "qrc:/qmlfiles/FirstRankList.qml");
	obs_data_set_default_string(settings, "themefont",
				    u8"阿里巴巴普惠体 M");
	obs_data_set_default_bool(settings, "themebold", false);
	obs_data_set_default_bool(settings, "themeitalic", false);

	obs_data_set_default_string(settings, "datafont", u8"阿里巴巴普惠体 R");
	obs_data_set_default_bool(settings, "databold", false);
	obs_data_set_default_bool(settings, "dataitalic", false);

	obs_data_set_default_int(settings, "themetype", 1);
	obs_data_set_default_int(settings, "listtype", 1); // 1:日版 2:周版
	obs_data_set_default_string(settings, "firstname", "");
	obs_data_set_default_string(settings, "avatarPath", "");

	obs_data_set_default_string(settings, "themefontcolor", "#FFFFFF");
	obs_data_set_default_string(settings, "datafontcolor", "#FFC80B");
	obs_data_set_default_int(settings, "transparence", 100);
}

static const char *firstranklist_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("quickfirstranklistshow");
}

static void firstranklist_source_update(void *data, obs_data_t *settings)
{
	FirstRankList *s = (FirstRankList *)data;
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

	int listtype = obs_data_get_int(settings, "listtype");
	s->setlisttype(listtype);

	const char *firstname = obs_data_get_string(settings, "firstname");
	s->setfirstname(firstname);

	const char *avatarpath = obs_data_get_string(settings, "avatarpath");
	s->setavatarpath(avatarpath);

	float transparence = (float)obs_data_get_int(settings, "transparence");
	transparence = transparence / 100.0f;
	s->settransparence(transparence);
}

static void *firstranklist_source_create(obs_data_t *settings,
					 obs_source_t *source)
{
	FirstRankList *context = new FirstRankList();
	context->setSource(source);

	firstranklist_source_update(context, settings);
	return context;
}

static void firstranklist_source_destroy(void *data)
{
	if (!data)
		return;
	FirstRankList *s = (FirstRankList *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void firstranklist_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 250);
	obs_data_set_default_int(settings, "height", 80);
	FirstRankList::default(settings);
}

static obs_properties_t *firstranklist_source_properties(void *data)
{
	if (!data)
		return nullptr;
	FirstRankList *s = (FirstRankList *)data;
	obs_properties_t *props = s->baseProperties();
	return props;
}

static void quickview_firstlistsource_make_custom(void *data,
						  obs_data_t *command)
{
	if (!data)
		return;

	const char *cmdType = obs_data_get_string(command, "type");
	if (strcmp("replay", cmdType) == 0) {
		FirstRankList *s = (FirstRankList *)data;
		emit s->replay();
	}
}

struct obs_source_info quickfirstranklistshow_source_info = {
	"quickfirstranklistshow_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION,
	firstranklist_source_get_name,
	firstranklist_source_create,
	firstranklist_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	firstranklist_source_defaults,
	firstranklist_source_properties,
	firstranklist_source_update,
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
	quickview_firstlistsource_make_custom};
