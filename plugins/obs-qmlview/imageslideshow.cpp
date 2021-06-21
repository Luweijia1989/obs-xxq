#include "imageslideshow.h"
#include "renderer.h"
#include "qmlhelper.h"

static const char *file_filter =
	"Image files (*.bmp *.tga *.png *.jpeg *.jpg *.gif)";

//QML_REGISTER_CREATABLE_TYPE(ImageSlideShow, ImageSlideShow)

ImageSlideShow::ImageSlideShow(QObject *parent /* = nullptr */)
	: QmlSourceBase(parent),
	  m_speed(SpeedSlow),
	  m_direction(BottomToTop),
	  m_horizontalAlignment(Qt::AlignHCenter),
	  m_verticalAlignment(Qt::AlignVCenter),
	  m_fillColor(QColor("#FF0000"))
{
	addProperties("imageSlideProperties", this);
}

void ImageSlideShow::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
				    "qrc:/qmlfiles/ImageSlideShow.qml");
	obs_data_set_default_int(settings, "speed", ImageSlideShow::SpeedSlow);
	obs_data_set_default_int(settings, "direction",
				 ImageSlideShow::BottomToTop);
	obs_data_set_default_string(settings, "fillColor", "#00000000");
	obs_data_set_default_int(settings, "horizontalAlignment",
				 Qt::AlignHCenter);
	obs_data_set_default_int(settings, "verticalAlignment",
				 Qt::AlignVCenter);

	obs_data_set_default_bool(settings, "isNew", false);
	obs_data_set_default_int(settings, "stopTime", 5000);
	obs_data_set_default_int(settings, "moveTime", 500);
}

static const char *quickview_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("quickimageslideshow");
}

static void quickview_source_update(void *data, obs_data_t *settings)
{
	ImageSlideShow *s = (ImageSlideShow *)data;
	s->baseUpdate(settings);

	quint32 speed = obs_data_get_int(settings, "speed");
	s->setspeed((ImageSlideShow::SlideSpeed)speed);

	quint32 direction = obs_data_get_int(settings, "direction");
	s->setdirection((ImageSlideShow::SlideDirection)direction);

	bool isNew = obs_data_get_bool(settings, "isNew");
	s->setisNew(isNew);

	int moveTime = obs_data_get_int(settings, "moveTime");
	s->setmoveTime(moveTime);

	int stopTime = obs_data_get_int(settings, "stopTime");
	s->setstopTime(stopTime);

	QStringList newFiles;
	obs_data_array_t *array = obs_data_get_array(settings, "imageUrls");
	int count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		const char *path = obs_data_get_string(item, "value");
		bool hidden = obs_data_get_bool(item, "hidden");
		if (!hidden)
			newFiles.append(QUrl::fromLocalFile(path).toString());
		obs_data_release(item);
	}
	obs_data_array_release(array);
	s->setimageFiles(newFiles);

	const char *color = obs_data_get_string(settings, "fillColor");
	s->setfillColor(QColor(color));

	quint32 halign = obs_data_get_int(settings, "horizontalAlignment");
	s->sethorizontalAlignment((Qt::Alignment)halign);

	quint32 valign = obs_data_get_int(settings, "verticalAlignment");
	s->setverticalAlignment((Qt::Alignment)valign);
}

static void *quickview_source_create(obs_data_t *settings, obs_source_t *source)
{
	ImageSlideShow *context = new ImageSlideShow();
	context->setSource(source);

	quickview_source_update(context, settings);
	return context;
}

static void quickview_source_destroy(void *data)
{
	if (!data)
		return;
	ImageSlideShow *s = (ImageSlideShow *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void quickview_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	ImageSlideShow::default(settings);
}

static obs_properties_t *quickview_source_properties(void *data)
{
	if (!data)
		return nullptr;
	ImageSlideShow *s = (ImageSlideShow *)data;
	obs_properties_t *props = s->baseProperties();

	obs_property_t *p = obs_properties_add_list(props, "direction",
						    u8"轮播动画",
						    OBS_COMBO_TYPE_LIST,
						    OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, u8"自下向上推进",
				  ImageSlideShow::BottomToTop);
	obs_property_list_add_int(p, u8"自上向下推进",
				  ImageSlideShow::TopToBottom);
	obs_property_list_add_int(p, u8"自右向左推进",
				  ImageSlideShow::RightToLeft);
	obs_property_list_add_int(p, u8"自左向右推进",
				  ImageSlideShow::LeftToRight);

	p = obs_properties_add_list(props, "speed", u8"轮播速度",
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, u8"慢速", ImageSlideShow::SpeedSlow);
	obs_property_list_add_int(p, u8"中速", ImageSlideShow::SpeedMiddle);
	obs_property_list_add_int(p, u8"快速", ImageSlideShow::SpeedFast);

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

	QString last_path;
	if (s) {
		s->m_mutex.lock();
		if (s->imageFiles().size()) {
			QFileInfo info(
				QUrl(s->imageFiles().last()).toLocalFile());
			last_path = info.absolutePath();
		}
		s->m_mutex.unlock();
	}

	obs_properties_add_editable_list(props, "imageUrls", u8"轮播元素列表",
					 OBS_EDITABLE_LIST_TYPE_FILES,
					 file_filter,
					 last_path.toUtf8().data());
	return props;
}

static void quickview_source_make_custom(void *data, obs_data_t *command)
{
	if (!data)
		return;

	const char *cmdType = obs_data_get_string(command, "type");
	if (strcmp("replay", cmdType) == 0) {
		ImageSlideShow *s = (ImageSlideShow *)data;
		emit s->replay();
	}
}

static struct obs_source_info quickimageslideshow_source_info = {
	"quickimageslideshow_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION,
	quickview_source_get_name,
	quickview_source_create,
	quickview_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	quickview_source_defaults,
	quickview_source_properties,
	quickview_source_update,
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
	quickview_source_make_custom};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("qml-source", "en-US")

//extern struct obs_source_info quicktextslideshow_source_info;
extern struct obs_source_info quickrank_source_info;
extern struct obs_source_info quickleave_source_info;
extern struct obs_source_info quickprivate_source_info;
extern struct obs_source_info quickaudiowave_source_info;
extern struct obs_source_info vote_source_info;
extern struct obs_source_info gifttv_source_info;
extern struct obs_source_info mask_source_info;
extern struct obs_source_info webcapture_source_info;
extern struct obs_source_info quickfanstargetshow_source_info;

bool obs_module_load(void)
{
	//obs_register_source(&quicktextslideshow_source_info);
	obs_register_source(&quickimageslideshow_source_info);
	obs_register_source(&quickrank_source_info);
	obs_register_source(&quickleave_source_info);
	obs_register_source(&quickprivate_source_info);
	obs_register_source(&quickaudiowave_source_info);
	obs_register_source(&vote_source_info);
	obs_register_source(&gifttv_source_info);
	obs_register_source(&mask_source_info);
	obs_register_source(&webcapture_source_info);
	obs_register_source(&quickfanstargetshow_source_info);
	return true;
}
