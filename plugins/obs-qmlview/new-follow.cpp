﻿#include "new-follow.h"
#include "renderer.h"
#include "qmlhelper.h"

//QML_REGISTER_CREATABLE_TYPE(TextSlideShow, TextSlideShow)

NewFollow::NewFollow(QObject *parent /* = nullptr */) : QmlSourceBase(parent)
{
	addProperties("newProperties", this);
	setuitype(2);
}

void NewFollow::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
				    "qrc:/qmlfiles/NewCommon.qml");
	obs_data_set_default_string(settings, "themefont",
				    u8"阿里巴巴普惠体 M");
	obs_data_set_default_bool(settings, "themebold", false);
	obs_data_set_default_bool(settings, "themeitalic", false);

	obs_data_set_default_string(settings, "datafont", u8"阿里巴巴普惠体 R");
	obs_data_set_default_bool(settings, "databold", false);
	obs_data_set_default_bool(settings, "dataitalic", false);

	obs_data_set_default_int(settings, "themetype", 1);
	obs_data_set_default_string(settings, "firstname", "");
	obs_data_set_default_string(settings, "avatarPath", "");

	obs_data_set_default_string(settings, "themefontcolor", "#FFFFFF");
	obs_data_set_default_string(settings, "datafontcolor", "#FFC80B");
	obs_data_set_default_int(settings, "transparence", 100);
	obs_data_set_default_bool(settings, "rewardLimitCheck", false);
	obs_data_set_default_int(settings, "rewardLimit", 10);
}

void NewFollow::setText(const QString &text)
{
	m_text = text;
}

QString NewFollow::text()
{
	return m_text;
}

static const char *newfollow_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("quicknewfollowshow");
}

static void newfollow_source_update(void *data, obs_data_t *settings)
{
	bool textChange = false;
	NewFollow *s = (NewFollow *)data;
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

	QString firstname = obs_data_get_string(settings, "firstname");
	s->setfirstname(firstname);

	if (s->text() != firstname) {
		s->setText(firstname);
		textChange = true;
	}

	const char *avatarpath = obs_data_get_string(settings, "avatarPath");
	s->setavatarpath(avatarpath);

	float transparence = (float)(qMax((int)obs_data_get_int(settings, "transparence"), 1));
	transparence = transparence / 100.0f;
	s->settransparence(transparence);

	if (textChange)
		emit s->update();
}

static void *newfollow_source_create(obs_data_t *settings, obs_source_t *source)
{
	NewFollow *context = new NewFollow();
	context->setSource(source);

	newfollow_source_update(context, settings);
	return context;
}

static void newfollow_source_destroy(void *data)
{
	if (!data)
		return;
	NewFollow *s = (NewFollow *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void newfollow_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 602);
	obs_data_set_default_int(settings, "height", 164);
	NewFollow::default(settings);
}

static obs_properties_t *newfollow_source_properties(void *data)
{
	if (!data)
		return nullptr;
	NewFollow *s = (NewFollow *)data;
	obs_properties_t *props = s->baseProperties();
	return props;
}

static void quickview_newfollowsource_make_custom(void *data,
						  obs_data_t *command)
{
	if (!data)
		return;

	const char *cmdType = obs_data_get_string(command, "type");
	if (strcmp("replay", cmdType) == 0) {
		NewFollow *s1 = (NewFollow *)data;
		emit s1->replay();
	} else if (strcmp("update", cmdType) == 0) {
		NewFollow *s2 = (NewFollow *)data;
		emit s2->update();
	}
}

struct obs_source_info quicknewfollowshow_source_info = {
	"quicknewfollowshow_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION,
	newfollow_source_get_name,
	newfollow_source_create,
	newfollow_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	newfollow_source_defaults,
	newfollow_source_properties,
	newfollow_source_update,
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
	quickview_newfollowsource_make_custom};
