#pragma once
#include "Common.h"
using namespace Gdiplus;
struct TimerSource {
	int opacity = 255;
	float animate_time = 0.10f;
	float update_time = 0.00f;
	int scale = 1;
	obs_source_t *source = nullptr;
	gs_texture_t *texture = nullptr;
	uint32_t cx = 0;
	uint32_t cy = 0;
	uint32_t text_posX = 0;
	uint32_t text_posY = 0;
	unique_ptr<Font> font = nullptr;
	HDCObj hdc;
	Gdiplus::Graphics graphics;
	wstring image_path;
	wstring text;
	wstring font_face;
	int face_size = 14;
	uint32_t color = 0xFFFFFFFF;

	time_t file_timestamp = 0;
	bool update_file = false;
	float update_time_elapsed = 0.0f;

	inline TimerSource(obs_source_t *source_, obs_data_t *settings)
		: source(source_),
		  hdc(CreateCompatibleDC(nullptr)),
		  graphics(hdc)
	{
		obs_source_update(source, settings);
	}

	inline ~TimerSource()
	{
		if (texture) {
			obs_enter_graphics();
			gs_texture_destroy(texture);
			obs_leave_graphics();
		}
	}
	void setopacity(int oPacity, Bitmap &imgTransform);
	void RenderGraphic();
	inline void Update(obs_data_t *settings);
	inline void Tick(float seconds);
	inline void Render();
};

void TimerSource::setopacity(int oPacity, Bitmap &imgTransform)
{
	int iWidth = imgTransform.GetWidth();
	int iHeight = imgTransform.GetHeight();

	Color color, colorTemp;

	for (int iRow = 0; iRow < iHeight; iRow++) {
		for (int iColumn = 0; iColumn < iWidth; iColumn++) {
			imgTransform.GetPixel(iColumn, iRow, &color);
			colorTemp.SetValue(color.MakeARGB(oPacity, color.GetR(),
							  color.GetG(),
							  color.GetB()));
			imgTransform.SetPixel(iColumn, iRow, colorTemp);
		}
	}
}

inline void TimerSource::Tick(float seconds)
{
	if (scale < 5)
		scale = scale + 1;
	else {
		if (opacity > 0) {
			opacity = opacity - 5 > 0 ? (opacity - 5) : 0;
			RenderGraphic();
		}
	}
}

inline void TimerSource::Render()
{
	if (!texture)
		return;

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"),
			      texture);
	gs_draw_sprite(texture, 0, cx * scale, cy * scale);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

void TimerSource::RenderGraphic()
{
	Status stat;
	SIZE size;
	size.cx = cx;
	size.cy = cy;
	unique_ptr<uint8_t> bits(new uint8_t[size.cx * size.cy * 4]);
	Bitmap bitmap(size.cx, size.cy, 4 * size.cx, PixelFormat32bppARGB,
		      bits.get());

	Gdiplus::Graphics graphics_bitmap(&bitmap);

	Color color1(0, 0, 0, 0);
	graphics_bitmap.Clear(color1);
	graphics_bitmap.SetTextRenderingHint(TextRenderingHintAntiAlias);
	graphics_bitmap.SetCompositingMode(CompositingModeSourceOver);
	graphics_bitmap.SetSmoothingMode(SmoothingModeAntiAlias);

	Image image(image_path.c_str());
	stat = graphics_bitmap.DrawImage(&image, 0, 0, size.cx, size.cy);
	//Image image1(L"D:/picture/ffff.jpeg");
	//Gdiplus::TextureBrush brush1(&image1);
	LinearGradientBrush brush(RectF(0, 0, (float)size.cx, (float)size.cy),
				  Color(calc_color(color, 100)),
				  Color(calc_color(color, 100)), 0, 1);

	//Matrix matrix;
	//matrix.Shear(-1.5f, 0.0f);
	//matrix.Scale(1, 0.5f);
	//matrix.Translate(30, 18);

	//graphics_bitmap.SetTransform(&matrix);
	//SolidBrush grayBrush(Color::Gray);

	//stat = graphics_bitmap.DrawString(text.c_str(), (int)text.size(),
	//				  font.get(), PointF(0.0f, 10.0f),
	//				  &grayBrush);
	//graphics_bitmap.ResetTransform();

	//SolidBrush brush(Color(color));
	PointF point;
	point.X = (float)text_posX;
	point.Y = (float)text_posY;

	stat = graphics_bitmap.DrawString(text.c_str(), (int)text.size(),
					  font.get(), point, &brush);

	if (opacity < 255 && opacity >= 0)
		setopacity(opacity, bitmap);

	if (!texture || (LONG)cx != size.cx || (LONG)cy != size.cy) {
		obs_enter_graphics();
		if (texture)
			gs_texture_destroy(texture);

		const uint8_t *data = (uint8_t *)bits.get();
		texture = gs_texture_create(size.cx, size.cy, GS_BGRA, 1, &data,
					    GS_DYNAMIC);

		obs_leave_graphics();

		cx = (uint32_t)size.cx;
		cy = (uint32_t)size.cy;

	} else if (texture) {
		obs_enter_graphics();
		gs_texture_set_image(texture, bits.get(), size.cx * 4, false);
		obs_leave_graphics();
	}
}

inline void TimerSource::Update(obs_data_t *s)
{
	const char *face = obs_data_get_string(s, "face");
	const char *imagePath = obs_data_get_string(s, "image_path");
	const char *textTmp = obs_data_get_string(s, "text");
	int newFaceSize = obs_data_get_int(s, "face_size");
	uint32_t posx = obs_data_get_uint32(s, "posx");
	uint32_t posy = obs_data_get_uint32(s, "posy");
	uint32_t width = obs_data_get_uint32(s, "width");
	uint32_t height = obs_data_get_uint32(s, "heigt");
	uint32_t colorTmp = obs_data_get_uint32(s, "text_color");
	wstring newImagePath = to_wide(imagePath);
	wstring newText = to_wide(textTmp);
	wstring newFontFace = to_wide(face);

	if (text_posX != posx)
		text_posX = posx;

	if (text_posY != posy)
		text_posY = posy;

	if (cx != width)
		cx = width;

	if (cy != height)
		cy = height;

	if (color != colorTmp)
		color = colorTmp;

	if (face_size != newFaceSize)
		face_size = newFaceSize;

	if (newFontFace != font_face && newFontFace.length() > 0)
		font_face = newFontFace;

	if (newText != text && newText.length() > 0)
		text = newText;

	if (newImagePath != image_path && newImagePath.length() > 0)
		image_path = newImagePath;

	font.reset(new Font(font_face.c_str(), face_size));
	RenderGraphic();
}
/* ------------------------------------------------------------------------- */
static obs_properties_t *get_properties(void *data)
{
	GiftSource *s = reinterpret_cast<GiftSource *>(data);
	obs_properties_t *props = obs_properties_create();
	return props;
}

static const char *timer_source_get_name(void *unused)
{
	return obs_module_text("GiftGDIPlus");
}

static void gift_source_update(void *data, obs_data_t *settings)
{
	reinterpret_cast<GiftSource *>(data)->Update(settings);
}

static void *gift_source_create(obs_data_t *settings, obs_source_t *source)
{
	return (void *)new GiftSource(source, settings);
}

static void gift_source_destroy(void *data)
{
	delete reinterpret_cast<GiftSource *>(data);
}

static obs_properties_t *gift_source_properties(void *unused)
{
	obs_properties_t *props = obs_properties_create();
	return props;
}

static void gift_source_render(void *data, gs_effect_t *effect)
{
	reinterpret_cast<GiftSource *>(data)->Render();
}

static void gift_source_defaults(obs_data_t *settings)
{
	obs_data_t *gift_obj = obs_data_create();
	obs_data_release(gift_obj);
}

static uint32_t gift_source_getwidth(void *data)
{
	return reinterpret_cast<GiftSource *>(data)->cx;
}

static uint32_t gift_source_getheight(void *data)
{
	return reinterpret_cast<GiftSource *>(data)->cy;
}

static void gift_source_tick(void *data, float seconds)
{
	reinterpret_cast<GiftSource *>(data)->Tick(seconds);
}

struct obs_source_info gift_source_info = {"gift_gdiplus",
					   OBS_SOURCE_TYPE_INPUT,
					   OBS_SOURCE_VIDEO |
						   OBS_SOURCE_CUSTOM_DRAW,
					   gift_source_get_name,
					   gift_source_create,
					   gift_source_destroy,
					   gift_source_getwidth,
					   gift_source_getheight,
					   gift_source_defaults,
					   gift_source_properties,
					   gift_source_update,
					   nullptr,
					   nullptr,
					   nullptr,
					   nullptr,
					   gift_source_tick,
					   gift_source_render,
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
					   nullptr};
