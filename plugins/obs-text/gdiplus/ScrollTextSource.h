#pragma once
#include "Common.h"
struct ScrollTextSource {
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
	uint32_t direction = LeftToRight;
	float update_time1 = 0.0002f;
	float update_time2 = 0.0000f;
	std::vector<string> texts;
	bool display = false;
	int curIndex = 0;
	SIZE max_size;
	float animate_time = 10.0f;
	bool replay = false;
	bool has_twotext = false;
	FontFamily families[1];
	Font *font_set;

	/* --------------------------- */
	inline ScrollTextSource(obs_source_t *source_, obs_data_t *settings)
		: source(source_),
		  hdc(CreateCompatibleDC(nullptr)),
		  graphics(hdc)
	{
		font_set = nullptr;
		obs_source_update(source, settings);
		max_size.cx = 0;
		max_size.cy = 0;
	}

	inline ~ScrollTextSource()
	{
		if (tex) {
			obs_enter_graphics();
			gs_texture_destroy(tex);
			obs_leave_graphics();
		}
	};

	void ScrollUpdate(obs_data_t *settings);
	void ScrollTextCustomCommand(obs_data_t *cmd);
	void ScrollTick(float seconds);
	void ScrollRender();
	void UpdateFont();
	void RenderText();

	void GetSlideStringFormat(StringFormat &format);
	void RemoveSlideNewlinePadding(const StringFormat &format, RectF &box);
	void CalculateTextSizes(const wstring &text_measure,
				const StringFormat &format, RectF &bounding_box,
				SIZE &text_size);
	void GenerateVerSlideText(const wstring &text_tansforms,
				  std::vector<wstring> &texts_vec);

	int CaculateTextColums(const wstring &text_measure);
	void CalculateTextPos(int count, const StringFormat &format,
			      float &posx, float &posy, const SIZE &size,
			      const float &posTime);
	void CalculateMaxSize(); // 计算最大长度
	void RenderSlideOutlineText(Graphics &graphics,
				    const GraphicsPath &path,
				    const Brush &brush);
	const char *GetMainSlideString(const char *str);
	void CaclculateSpeed();
	bool NeedRender();
	bool VerDeleteLineLarge();
};
