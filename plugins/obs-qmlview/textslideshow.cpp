#include "textslideshow.h"
#include "renderer.h"
#include "qmlhelper.h"

//QML_REGISTER_CREATABLE_TYPE(TextSlideShow, TextSlideShow)

TextSlideShow::TextSlideShow(QObject *parent /* = nullptr */)
	: QmlSourceBase(parent),
	  m_speed(SpeedSlow),
	  m_direction(BottomToTop),
	  m_horizontalAlignment(Qt::AlignHCenter),
	  m_verticalAlignment(Qt::AlignVCenter),
	  m_fillColor(QColor("#00000000"))
{
	addProperties("textSlideProperties", this);
}

void TextSlideShow::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
				    "qrc:/qmlfiles/TextSlideShow.qml");
	obs_data_set_default_int(settings, "speed", TextSlideShow::SpeedSlow);
	obs_data_set_default_int(settings, "direction",
				 TextSlideShow::BottomToTop);
	obs_data_set_default_string(settings, "fillColor", "#00000000");
	obs_data_set_default_string(settings, "frontColor", "#FFFFFF");
	obs_data_set_default_int(settings, "horizontalAlignment",
				 Qt::AlignLeft);
	obs_data_set_default_int(settings, "verticalAlignment",
				 Qt::AlignTop);
	obs_data_set_default_int(settings, "fontSize", 36);
	obs_data_set_default_bool(settings, "hDisplay", true);
	obs_data_set_default_string(settings, "font", u8"微软雅黑");
}

static const char *textslide_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("quicktextslideshow");
}

static void textslide_source_update(void *data, obs_data_t *settings)
{
	TextSlideShow *s = (TextSlideShow *)data;
	s->baseUpdate(settings);

	quint32 speed = obs_data_get_int(settings, "speed");
	s->setspeed((TextSlideShow::SlideSpeed)speed);

	quint32 direction = obs_data_get_int(settings, "direction");
	s->setdirection((TextSlideShow::SlideDirection)direction);

	QStringList texts;
	obs_data_array_t *array = obs_data_get_array(settings, "textStrings");
	int count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		const char *path = obs_data_get_string(item, "value");
		bool hidden = obs_data_get_bool(item, "hidden");
		if (!hidden)
			texts.append(path);
		obs_data_release(item);
	}
	obs_data_array_release(array);
	s->settextStrings(texts);

	const char *color = obs_data_get_string(settings, "fillColor");
	s->setfillColor(QColor(color));

	quint32 halign = obs_data_get_int(settings, "horizontalAlignment");
	s->sethorizontalAlignment((Qt::Alignment)halign);

	quint32 valign = obs_data_get_int(settings, "verticalAlignment");
	s->setverticalAlignment((Qt::Alignment)valign);

	const char *front_color = obs_data_get_string(settings, "frontColor");
	s->setfrontColor(QColor(front_color));

	bool bold = obs_data_get_bool(settings, "bold");
	s->setbold(bold);

	bool italic = obs_data_get_bool(settings, "italic");
	s->setitalic(italic);

	bool strikeout = obs_data_get_bool(settings, "strikeout");
	s->setstrikeout(strikeout);

	bool underline = obs_data_get_bool(settings, "underline");
	s->setunderline(underline);

	const char *font = obs_data_get_string(settings, "font");
	s->setfont(font);

	int fontSize = obs_data_get_int(settings, "fontSize");
	s->setfontSize(fontSize);

	bool hDisplay = obs_data_get_bool(settings, "hDisplay");
	s->sethDisplay(hDisplay);
}

static void *textslide_source_create(obs_data_t *settings, obs_source_t *source)
{
	TextSlideShow *context = new TextSlideShow();
	context->setSource(source);

	textslide_source_update(context, settings);
	return context;
}

static void textslide_source_destroy(void *data)
{
	if (!data)
		return;
	TextSlideShow *s = (TextSlideShow *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void textslide_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 380);
	obs_data_set_default_int(settings, "height", 380);
	TextSlideShow::default(settings);
}

static obs_properties_t *textslide_source_properties(void *data)
{
	if (!data)
		return nullptr;
	TextSlideShow *s = (TextSlideShow *)data;
	obs_properties_t *props = s->baseProperties();

	obs_property_t *p = obs_properties_add_list(props, "direction",
						    u8"轮播动画",
						    OBS_COMBO_TYPE_LIST,
						    OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, u8"自下向上推进",
				  TextSlideShow::BottomToTop);
	obs_property_list_add_int(p, u8"自上向下推进",
				  TextSlideShow::TopToBottom);
	obs_property_list_add_int(p, u8"自右向左推进",
				  TextSlideShow::RightToLeft);
	obs_property_list_add_int(p, u8"自左向右推进",
				  TextSlideShow::LeftToRight);

	p = obs_properties_add_list(props, "speed", u8"轮播速度",
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, u8"慢速", TextSlideShow::SpeedSlow);
	obs_property_list_add_int(p, u8"中速", TextSlideShow::SpeedMiddle);
	obs_property_list_add_int(p, u8"快速", TextSlideShow::SpeedFast);

	p = obs_properties_add_list(props, "horizontalAlignment", u8"水平对齐",
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, u8"水平靠左", Qt::AlignLeft);
	obs_property_list_add_int(p, u8"水平居中", Qt::AlignHCenter);
	obs_property_list_add_int(p, u8"水平靠右", Qt::AlignRight);

	p = obs_properties_add_list(props, "verticalAlignment", u8"垂直对齐",
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, u8"垂直靠上", Qt::AlignTop);
	obs_property_list_add_int(p, u8"垂直居中", Qt::AlignVCenter);
	obs_property_list_add_int(p, u8"垂直靠下", Qt::AlignBottom);

	obs_properties_add_text(props, "fillColor", u8"填充颜色",
				OBS_TEXT_DEFAULT);

	obs_properties_add_text(props, "frontColor", u8"文字颜色",
				OBS_TEXT_DEFAULT);

	obs_properties_add_bool(props, "bold", u8"粗体");
	obs_properties_add_bool(props, "italic", u8"斜体");
	obs_properties_add_bool(props, "strikeout", u8"删除线");
	obs_properties_add_bool(props, "underline", u8"下划线");
	obs_properties_add_bool(props, "hDisplay", u8"文字方向");

	obs_properties_add_text(props, "font", u8"字体", OBS_TEXT_DEFAULT);
	obs_properties_add_int(props, "fontSize", u8"字体大小", 12, 72, 1);

	obs_properties_add_editable_list(props, "textStrings", u8"轮播文字列表",
					 OBS_EDITABLE_LIST_TYPE_STRINGS,
					 nullptr,
					 nullptr);
	return props;
}

struct obs_source_info quicktextslideshow_source_info = {
	"quicktextslideshow_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION,
	textslide_source_get_name,
	textslide_source_create,
	textslide_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	textslide_source_defaults,
	textslide_source_properties,
	textslide_source_update,
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
