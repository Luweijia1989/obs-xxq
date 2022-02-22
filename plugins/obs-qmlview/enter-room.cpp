#include "enter-room.h"
#include "renderer.h"
#include "qmlhelper.h"

//QML_REGISTER_CREATABLE_TYPE(TextSlideShow, TextSlideShow)

EnterRoom::EnterRoom(QObject *parent /* = nullptr */) : QmlSourceBase(parent)
{
	addProperties("newProperties", this);
	setuitype(5);

	connect(&m_timer, &QTimer::timeout, this, [=]() {
		if (!m_canstay) {
			doHide();
		}
	});
}

EnterRoom::~EnterRoom()
{
	if (m_timer.isActive()) {
		m_timer.stop();
	}
}

void EnterRoom::default(obs_data_t *settings)
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

	obs_data_set_default_int(settings, "themetype", 3);
	obs_data_set_default_string(settings, "firstname", "");
	obs_data_set_default_string(settings, "avatarPath", "");

	obs_data_set_default_string(settings, "themefontcolor", "#FFFFFF");
	obs_data_set_default_string(settings, "datafontcolor", "#FFC80B");
	obs_data_set_default_int(settings, "transparence", 100);

	obs_data_set_default_int(settings, "staytime", 10);
	obs_data_set_default_bool(settings, "canstay", false);
}

void EnterRoom::setText(const QString &text)
{
	m_text = text;
}

void EnterRoom::doHide()
{
	emit hide();
	setavatarpath("");
	m_text = u8"用户昵称";
	setfirstname(m_text);
}

void EnterRoom::doStart(bool isenter)
{
	int costTime = m_staytime;
	if (m_timer.isActive()) {
		m_timer.stop();
	}

	if (isenter)
		emit enter();
	else
		emit show();
	m_timer.setInterval(m_staytime * 1000);
	m_timer.setSingleShot(true);
	m_timer.start();
}

QString EnterRoom::text()
{
	return m_text;
}

static const char *enterRoom_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("quickenterroomshow");
}

static void enterRoom_source_update(void *data, obs_data_t *settings)
{
	bool textChange = false;
	EnterRoom *s = (EnterRoom *)data;
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
	if (s->themetype() != themeType) {
		textChange = true;
	}
	s->setthemetype(themeType);

	QString firstname = obs_data_get_string(settings, "firstname");
	s->setfirstname(firstname);

	if (s->text() != firstname) {
		s->setText(firstname);
		textChange = true;
	}

	const char *avatarpath = obs_data_get_string(settings, "avatarPath");
	s->setavatarpath(avatarpath);

	float transparence = (float)obs_data_get_int(settings, "transparence");
	transparence = transparence / 100.0f;
	s->settransparence(transparence);

	int stayTime = obs_data_get_int(settings, "staytime");
	s->setstaytime(stayTime);
	bool canStay = obs_data_get_bool(settings, "canstay");
	s->setcanstay(canStay);
	if (textChange)
		emit s->update();
}

static void *enterRoom_source_create(obs_data_t *settings, obs_source_t *source)
{
	EnterRoom *context = new EnterRoom();
	context->setSource(source);

	enterRoom_source_update(context, settings);
	return context;
}

static void enterRoom_source_destroy(void *data)
{
	if (!data)
		return;
	EnterRoom *s = (EnterRoom *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void enterRoom_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 542);
	obs_data_set_default_int(settings, "height", 247);
	EnterRoom::default(settings);
}

static obs_properties_t *enterRoom_source_properties(void *data)
{
	if (!data)
		return nullptr;
	EnterRoom *s = (EnterRoom *)data;
	obs_properties_t *props = s->baseProperties();
	return props;
}

static void quickview_enterRoomsource_make_custom(void *data,
						  obs_data_t *command)
{
	if (!data)
		return;

	const char *cmdType = obs_data_get_string(command, "type");
	if (strcmp("replay", cmdType) == 0) {
		EnterRoom *s1 = (EnterRoom *)data;
		emit s1->replay();
		s1->doStart(false);
	} else if (strcmp("update", cmdType) == 0) {
		EnterRoom *s2 = (EnterRoom *)data;
		emit s2->update();
	} else if (strcmp("enterUpdate", cmdType) == 0) {
		EnterRoom *s3 = (EnterRoom *)data;
		s3->doStart(true);
	} else if (strcmp("hide", cmdType) == 0) {
		EnterRoom *s4 = (EnterRoom *)data;
		emit s4->doHide();
	}
}

struct obs_source_info quickenterroomshow_source_info = {
	"quickenterroom_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION,
	enterRoom_source_get_name,
	enterRoom_source_create,
	enterRoom_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	enterRoom_source_defaults,
	enterRoom_source_properties,
	enterRoom_source_update,
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
	quickview_enterRoomsource_make_custom};
