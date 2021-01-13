#pragma once
#include "Common.h"
struct SlideTextSource {
	obs_source_t *source = nullptr;
	gs_texture_t *tex = nullptr;
	uint32_t cx = 0;
	uint32_t cy = 0;
	HDCObj hdc;
	Graphics graphics;
	HFONTObj hfont;
	unique_ptr<Font> font;
	float update_time_elapsed = 0.0f;
	wstring text;
	wstring face;
	int face_size = 0;
	uint32_t color = 0xFFFFFF;
	uint32_t color2 = 0xFFFFFF;
	float gradient_dir = 0;
	uint32_t opacity = 100;
	uint32_t opacity2 = 100;
	uint32_t bk_color = 0;
	uint32_t bk_opacity = 100;
	Align align = Align::Left;
	VAlign valign = VAlign::Top;
	bool gradient = false;
	bool bold = false;
	bool italic = false;
	bool underline = false;
	bool strikeout = false;
	bool vertical = false;
	bool use_outline = false;
	float outline_size = 0.0f;
	uint32_t outline_color = 0;
	uint32_t outline_opacity = 100;

	bool use_extents = false;
	bool wrap = false;
	uint32_t extents_cx = 0;
	uint32_t extents_cy = 0;

	int text_transform = S_TRANSFORM_NONE;

	bool chatlog_mode = false;
	int chatlog_lines = 6;
	uint32_t speed = 1;
	uint32_t direction = BottomToTop;
	float update_time = 0.00f;
	std::vector<string> texts;
	bool display = false;
	int curIndex = 0;
	SIZE max_size;
	float animate_time = 3.0f;
	bool replay = false;

	FontFamily families[1];
	Font *font_set;

	std::vector<wstring> ver_texts;

	/* --------------------------- */

	inline SlideTextSource(obs_source_t *source_, obs_data_t *settings)
		: source(source_),
		  hdc(CreateCompatibleDC(nullptr)),
		  graphics(hdc)
	{
		texts.clear();
		font_set = nullptr;
		obs_source_update(source, settings);
		max_size.cx = 0;
		max_size.cy = 0;
	}

	inline ~SlideTextSource()
	{
		if (tex) {
			obs_enter_graphics();
			gs_texture_destroy(tex);
			obs_leave_graphics();
		}
	}
	void UpdateSlideFont();
	void GetSlideStringFormat(StringFormat &format);
	void RemoveSlideNewlinePadding(const StringFormat &format, RectF &box);
	void CalculateSlideTextSizes(const wstring &text_measure,
				     const StringFormat &format,
				     RectF &bounding_box, SIZE &text_size);
	void GenerateVerSlideText(const wstring &text_tansforms,
				  std::vector<wstring> &texts_vec);

	int CaculateSlideTextColums(const wstring &text_measure);
	void CalculateSlideTextPos(int count, const StringFormat &format,
				   float &posx, float &posy, const SIZE &size);
	void CalculateMaxSize(); // 计算最大长度
	void RenderSlideOutlineText(Graphics &graphics,
				    const GraphicsPath &path,
				    const Brush &brush);
	void RenderSlideText();
	const char *GetMainSlideString(const char *str);

	void SlideUpdate(obs_data_t *settings);
	void SlideTextCustomCommand(obs_data_t *cmd);
	void SlideTick(float seconds);
	void SlideRender();
	void CaclculateSpeed();
	bool NeedRender();
	wstring GetNextString();
	bool VerDeleteLineLarge();
};
