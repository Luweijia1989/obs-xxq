#include "webcapture.h"
#include <QDebug>

WebCapture::WebCapture(QString memoryName)
	: m_source(nullptr), m_imageTexture(nullptr), width(0), height(0)
{
}

WebCapture::~WebCapture()
{
	if (m_imageTexture) {
		obs_enter_graphics();
		gs_texture_destroy(m_imageTexture);
		obs_leave_graphics();
	}
}

void WebCapture::createTexture(int w, int h)
{
	width = w;
	height = h;
	obs_enter_graphics();
	m_imageTexture =
		gs_texture_create(width, height, GS_BGRA, 1, NULL, GS_DYNAMIC);
	obs_leave_graphics();
}

void WebCapture::updateTextureData(uint8_t *data)
{
	obs_enter_graphics();
	gs_texture_set_image(m_imageTexture, data, width * 4, false);
	obs_leave_graphics();
}

static const char *webcapture_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("webcapture-source");
}

static void webcapture_source_update(void *data, obs_data_t *settings) {}

static void *webcapture_source_create(obs_data_t *settings,
				      obs_source_t *source)
{
	WebCapture *capture =
		new WebCapture(obs_data_get_string(settings, "memroyName"));
	capture->m_source = source;
	return capture;
}

static void webcapture_source_destroy(void *data)
{
	if (!data)
		return;
	WebCapture *capture = (WebCapture *)data;
	delete capture;
}

static void webcapture_source_defaults(obs_data_t *settings) {}

static obs_properties_t *webcapture_source_properties(void *data)
{
	return nullptr;
}

static uint32_t webcapture_source_getwidth(void *data)
{
	if (!data)
		return 0;
	WebCapture *capture = (WebCapture *)data;
	return capture->width;
}

static uint32_t webcapture_source_getheight(void *data)
{
	if (!data)
		return 0;
	WebCapture *capture = (WebCapture *)data;
	return capture->height;
}

static void webcapture_source_show(void *data) {}

static void webcapture_source_hide(void *data) {}

static void webcapture_source_render(void *data, gs_effect_t *effect)
{
	if (!data)
		return;
	WebCapture *capture = (WebCapture *)data;

	if (!capture->m_imageTexture) {
		return;
	}

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"),
			      capture->m_imageTexture);
	obs_source_draw(capture->m_imageTexture, 0, 0, 0, 0, false);
}

static void webcapture_command(void *data, obs_data_t *command)
{
	if (!data)
		return;
	WebCapture *capture = (WebCapture *)data;

	const char *type = obs_data_get_string(command, "type");
	if (strcmp(type, "create") == 0) {
		capture->createTexture(obs_data_get_int(command, "width"), obs_data_get_int(command, "height"));
	} else if (strcmp(type, "update") == 0) {
		capture->updateTextureData((uint8_t*)obs_data_get_string(command, "imageData"));
	}
}

struct obs_source_info webcapture_source_info = {
	"webcapture_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_DO_NOT_DUPLICATE,
	webcapture_source_get_name,
	webcapture_source_create,
	webcapture_source_destroy,
	webcapture_source_getwidth,
	webcapture_source_getheight,
	webcapture_source_defaults,
	webcapture_source_properties,
	webcapture_source_update,
	nullptr,
	nullptr,
	webcapture_source_show,
	webcapture_source_hide,
	nullptr,
	webcapture_source_render,
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
	webcapture_command};
