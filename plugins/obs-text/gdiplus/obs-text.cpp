﻿#include "SlideTextSource.h"
#include "ScrollTextSource.h"
#include "GauseBlur.h"
#include "Common.h"
struct TextSource {
	obs_source_t *source = nullptr;
	gs_texture_t *tex = nullptr;
	uint32_t cx = 0;
	uint32_t cy = 0;

	HDCObj hdc;
	Graphics graphics;

	HFONTObj hfont;
	unique_ptr<Font> font;

	bool read_from_file = false;
	string file;
	time_t file_timestamp = 0;
	bool update_file = false;
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
	uint32_t bk_opacity = 0;
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

	FontFamily families[1];
	Font *font_set;
	bool special_source = false;

	// 阴影相关
	bool use_shadow = true;
	uint32_t shadow_color = 0x000000;
	int shadow_opacity = 50;
	float shadow_distance = 2.0f;
	int shadow_dir = 0; // 0:左下 1：左上 2：右下 3：右上
	/* --------------------------- */

	inline TextSource(obs_source_t *source_, obs_data_t *settings)
		: source(source_),
		  hdc(CreateCompatibleDC(nullptr)),
		  graphics(hdc)
	{
		font_set = nullptr;
		obs_source_update(source, settings);
	}

	inline ~TextSource()
	{
		if (tex) {
			obs_enter_graphics();
			gs_texture_destroy(tex);
			obs_leave_graphics();
		}
	}
	void UpdateFont();
	void GetStringFormat(StringFormat &format);
	void RemoveNewlinePadding(const StringFormat &format, RectF &box);
	void CalculateTextSizes(const StringFormat &format, RectF &bounding_box,
				SIZE &text_size);
	void RenderOutlineText(Graphics &graphics, const GraphicsPath &path,
			       const Brush &brush);
	void RenderOutlineShadowText(Graphics &graphics,
				     const GraphicsPath &path,
				     const Brush &brush);
	void RenderText();
	void RenderShadow(const RectF &box, const SIZE &size,
			  const StringFormat &format, Graphics &graphics);
	void LoadFileText();
	void CaculateShadowDis(float &dx, float &dy);

	const char *GetMainString(const char *str);

	inline void Update(obs_data_t *settings);
	inline void Tick(float seconds);
	inline void Render();
};

void TextSource::CaculateShadowDis(float &dx, float &dy)
{
	if (shadow_dir == 1) {
		dx = -abs(dx);
		dy = -abs(dy);
	} else if (shadow_dir == 0) {
		dx = -abs(dx);
		dy = abs(dy);
	} else if (shadow_dir == 3) {
		dx = abs(dx);
		dy = -abs(dy);
	} else if (shadow_dir == 2) {
		dx = abs(dx);
		dy = abs(dy);
	}
}

void TextSource::UpdateFont()
{
	hfont = nullptr;
	font.reset(nullptr);
	if (face == L"阿里汉仪智能黑体" || face == L"DIN Condensed" ||
	    face == L"阿里巴巴普惠体 R" || face == L"阿里巴巴普惠体 M" ||
	    face == L"DIN-BoldItalic" || face == L"DIN Alternate") {
		bool bInstall = IsInstallFont(face.c_str());
		if (bInstall == false) {
			wstring fontpath = FontPath(face.c_str());
			if (fontpath == L"")
				return;
			PrivateFontCollection fontCollection;
			wchar_t cwd[MAX_PATH];
			GetModuleFileNameW(nullptr, cwd, _countof(cwd) - 1);
			wchar_t *p = wcsrchr(cwd, '\\');
			if (p)
				*p = 0;
			wstring path = cwd;
			path = path + fontpath;
			Status result =
				fontCollection.AddFontFile(path.c_str());
			if (result != Ok)
				goto Normal_Set;

			int numFamilies;
			fontCollection.GetFamilies(1, families, &numFamilies);
			int style = FontStyleRegular;
			if (bold)
				style = style | FontStyleBold;
			if (italic)
				style = style | FontStyleItalic;

			if (underline)
				style = style | FontStyleUnderline;

			if (strikeout)
				style = style | FontStyleStrikeout;

			font_set = new Font(&families[0],
					    face_size * 2.0f / 2.7f, style,
					    UnitPixel);
			font.reset(font_set);
			return;
		}
	}

Normal_Set:
	LOGFONT lf = {};
	lf.lfHeight = face_size;
	lf.lfWeight = bold ? FW_BOLD : FW_DONTCARE;
	lf.lfItalic = italic;
	lf.lfUnderline = underline;
	lf.lfStrikeOut = strikeout;
	lf.lfQuality = ANTIALIASED_QUALITY;
	lf.lfCharSet = DEFAULT_CHARSET;

	if (!face.empty()) {
		wcscpy(lf.lfFaceName, face.c_str());
		hfont = CreateFontIndirect(&lf);
	}

	if (!hfont) {
		wcscpy(lf.lfFaceName, L"Arial");
		hfont = CreateFontIndirect(&lf);
	}

	if (hfont) {
		font.reset(new Font(hdc, hfont));
		font->GetFamily(&families[0]);
	}
}

void TextSource::GetStringFormat(StringFormat &format)
{
	UINT flags = StringFormatFlagsNoFitBlackBox |
		     StringFormatFlagsMeasureTrailingSpaces;

	if (vertical)
		flags |= StringFormatFlagsDirectionVertical |
			 StringFormatFlagsDirectionRightToLeft;

	format.SetFormatFlags(flags);
	format.SetTrimming(StringTrimmingWord);

	switch (align) {
	case Align::Left:
		if (vertical)
			format.SetLineAlignment(StringAlignmentFar);
		else
			format.SetAlignment(StringAlignmentNear);
		break;
	case Align::Center:
		if (vertical)
			format.SetLineAlignment(StringAlignmentCenter);
		else
			format.SetAlignment(StringAlignmentCenter);
		break;
	case Align::Right:
		if (vertical)
			format.SetLineAlignment(StringAlignmentNear);
		else
			format.SetAlignment(StringAlignmentFar);
	}

	switch (valign) {
	case VAlign::Top:
		if (vertical)
			format.SetAlignment(StringAlignmentNear);
		else
			format.SetLineAlignment(StringAlignmentNear);
		break;
	case VAlign::Center:
		if (vertical)
			format.SetAlignment(StringAlignmentCenter);
		else
			format.SetLineAlignment(StringAlignmentCenter);
		break;
	case VAlign::Bottom:
		if (vertical)
			format.SetAlignment(StringAlignmentFar);
		else
			format.SetLineAlignment(StringAlignmentFar);
	}
}

/* GDI+ treats '\n' as an extra character with an actual render size when
 * calculating the texture size, so we have to calculate the size of '\n' to
 * remove the padding.  Because we always add a newline to the string, we
 * also remove the extra unused newline. */
void TextSource::RemoveNewlinePadding(const StringFormat &format, RectF &box)
{
	RectF before;
	RectF after;
	Status stat;

	stat = graphics.MeasureString(L"W", 2, font.get(), PointF(0.0f, 0.0f),
				      &format, &before);
	warn_stat("MeasureString (without newline)");

	stat = graphics.MeasureString(L"W\n", 3, font.get(), PointF(0.0f, 0.0f),
				      &format, &after);
	warn_stat("MeasureString (with newline)");

	float offset_cx = after.Width - before.Width;
	float offset_cy = after.Height - before.Height;

	if (!vertical) {
		if (offset_cx >= 1.0f)
			offset_cx -= 1.0f;

		if (valign == VAlign::Center)
			box.Y -= offset_cy * 0.5f;
		else if (valign == VAlign::Bottom)
			box.Y -= offset_cy;
	} else {
		if (offset_cy >= 1.0f)
			offset_cy -= 1.0f;

		if (align == Align::Center)
			box.X -= offset_cx * 0.5f;
		else if (align == Align::Right)
			box.X -= offset_cx;
	}

	box.Width -= offset_cx;
	box.Height -= offset_cy;
}

void TextSource::CalculateTextSizes(const StringFormat &format,
				    RectF &bounding_box, SIZE &text_size)
{
	RectF layout_box;
	RectF temp_box;
	Status stat;

	if (!text.empty()) {
		if (use_extents && wrap) {
			layout_box.X = layout_box.Y = 0;
			layout_box.Width = float(extents_cx);
			layout_box.Height = float(extents_cy);

			if (use_outline) {
				layout_box.Width -= outline_size;
				layout_box.Height -= outline_size;
			}

			stat = graphics.MeasureString(text.c_str(),
						      (int)text.size() + 1,
						      font.get(), layout_box,
						      &format, &bounding_box);
			warn_stat("MeasureString (wrapped)");

			temp_box = bounding_box;
		} else {
			stat = graphics.MeasureString(
				text.c_str(), (int)text.size() + 1, font.get(),
				PointF(0.0f, 0.0f), &format, &bounding_box);
			warn_stat("MeasureString (non-wrapped)");

			temp_box = bounding_box;

			bounding_box.X = 0.0f;
			bounding_box.Y = 0.0f;

			RemoveNewlinePadding(format, bounding_box);

			if (use_outline) {
				bounding_box.Width += outline_size;
				bounding_box.Height += outline_size;
			}
		}
	}

	if (vertical) {
		if (bounding_box.Width < face_size) {
			text_size.cx = face_size;
			bounding_box.Width = float(face_size);
		} else {
			text_size.cx = LONG(bounding_box.Width + EPSILON);
		}

		text_size.cy = LONG(bounding_box.Height + EPSILON);
	} else {
		if (bounding_box.Height < face_size) {
			text_size.cy = face_size;
			bounding_box.Height = float(face_size);
		} else {
			text_size.cy = LONG(bounding_box.Height + EPSILON);
		}

		text_size.cx = LONG(bounding_box.Width + EPSILON);
	}

	if (use_extents) {
		text_size.cx = extents_cx;
		text_size.cy = extents_cy;
	}

	text_size.cx += text_size.cx % 2;
	text_size.cy += text_size.cy % 2;

	int64_t total_size = int64_t(text_size.cx) * int64_t(text_size.cy);

	/* GPUs typically have texture size limitations */
	clamp(text_size.cx, MIN_SIZE_CX, MAX_SIZE_CX);
	clamp(text_size.cy, MIN_SIZE_CY, MAX_SIZE_CY);

	/* avoid taking up too much VRAM */
	if (total_size > MAX_AREA) {
		if (text_size.cx > text_size.cy)
			text_size.cx = (LONG)MAX_AREA / text_size.cy;
		else
			text_size.cy = (LONG)MAX_AREA / text_size.cx;
	}

	/* the internal text-rendering bounding box for is reset to
	 * its internal value in case the texture gets cut off */
	bounding_box.Width = temp_box.Width;
	bounding_box.Height = temp_box.Height;

	if (special_source) {
		RectF rectFont;
		Status stat;
		stat = graphics.MeasureString(L"大", 2, font.get(),
					      PointF(0.0f, 0.0f), &format,
					      &rectFont);
		float diff = bounding_box.Width - text_size.cx;
		text_size.cx = rectFont.Width * 10;
		bounding_box.Width = text_size.cx + diff;
	}
}

void TextSource::RenderOutlineShadowText(Graphics &graphics,
					 const GraphicsPath &path,
					 const Brush &brush)
{
	DWORD outline_rgba = calc_color(shadow_color, shadow_opacity);
	Status stat;

	Pen pen(Color(outline_rgba), outline_size);
	stat = pen.SetLineJoin(LineJoinRound);
	warn_stat("penshaow.SetLineJoin");

	stat = graphics.DrawPath(&pen, &path);
	warn_stat("graphicsshaow.DrawPath");

	stat = graphics.FillPath(&brush, &path);
	warn_stat("graphicsshaow.FillPath");
}

void TextSource::RenderOutlineText(Graphics &graphics, const GraphicsPath &path,
				   const Brush &brush)
{
	DWORD outline_rgba = calc_color(outline_color, outline_opacity);
	Status stat;

	Pen pen(Color(outline_rgba), outline_size);
	stat = pen.SetLineJoin(LineJoinRound);
	warn_stat("pen.SetLineJoin");

	stat = graphics.DrawPath(&pen, &path);
	warn_stat("graphics.DrawPath");

	stat = graphics.FillPath(&brush, &path);
	warn_stat("graphics.FillPath");
}

void TextSource::RenderShadow(const RectF &box, const SIZE &size,
			      const StringFormat &format, Graphics &graphics)
{
	if (!use_shadow)
		return;
	RectF box1 = box;
	LONG width = size.cx;
	LONG height = size.cy;
	if (!text.empty()) {
		float dx = shadow_distance;
		float dy = shadow_distance;
		DWORD color1 = shadow_color & 0xFFFFFF;
		color1 |= get_alpha_val(shadow_opacity);
		CaculateShadowDis(dx, dy);
		SolidBrush bk_brush = Color(color1);
		if (use_outline) {
			box1.Offset(dx, dy);
			GraphicsPath path;
			path.AddString(text.c_str(), (int)text.size(),
				       &families[0], font->GetStyle(),
				       font->GetSize(), box1, &format);
			RenderOutlineShadowText(graphics, path, bk_brush);

		} else {
			graphics.SetTextRenderingHint(
				TextRenderingHintAntiAlias);
			Matrix mx(1.0f, 0, 0, 1.0f, dx, dy);
			graphics.SetTransform(&mx);
			graphics.DrawString(text.c_str(), (int)text.size(),
					    font.get(), box1, &format,
					    &bk_brush);
			Matrix mx1(1.0f, 0, 0, 1.0f, -dx, -dy);
			graphics.SetTransform(&mx1);
		}
	}
}

void TextSource::RenderText()
{
	StringFormat format(StringFormat::GenericTypographic());
	Status stat;

	RectF box;
	SIZE size;

	GetStringFormat(format);
	CalculateTextSizes(format, box, size);
	unique_ptr<uint8_t> bits(new uint8_t[size.cx * size.cy * 4]);
	Bitmap bitmap(size.cx, size.cy, 4 * size.cx, PixelFormat32bppARGB,
		      bits.get());

	Graphics graphics_bitmap(&bitmap);
	LinearGradientBrush brush(RectF(0, 0, (float)size.cx, (float)size.cy),
				  Color(calc_color(color, opacity)),
				  Color(calc_color(color2, opacity2)),
				  gradient_dir, 1);
	DWORD full_bk_color = bk_color & 0xFFFFFF;
	if (!text.empty() || use_extents)
		full_bk_color |= get_alpha_val(bk_opacity);

	if ((size.cx > box.Width || size.cy > box.Height) && !use_extents) {
		stat = graphics_bitmap.Clear(Color(0));
		warn_stat("graphics_bitmap.Clear");

		SolidBrush bk_brush = Color(full_bk_color);
		stat = graphics_bitmap.FillRectangle(&bk_brush, box);
		warn_stat("graphics_bitmap.FillRectangle");
	} else {
		stat = graphics_bitmap.Clear(Color(full_bk_color));
		warn_stat("graphics_bitmap.Clear");
	}

	graphics_bitmap.SetTextRenderingHint(TextRenderingHintAntiAlias);
	graphics_bitmap.SetCompositingMode(CompositingModeSourceOver);
	graphics_bitmap.SetSmoothingMode(SmoothingModeAntiAlias);

	if (use_shadow) {
		RenderShadow(box, size, format, graphics_bitmap);
		UINT *dataShadow = (UINT *)bits.get();
		GauseBlur gauseBlur(5.0, 2);
		gauseBlur.Do(dataShadow, size.cx, size.cy);
	}
	if (!text.empty()) {
		if (use_outline) {
			box.Offset(outline_size / 2, outline_size / 2);
			GraphicsPath path;
			stat = path.AddString(text.c_str(), (int)text.size(),
					      &families[0], font->GetStyle(),
					      font->GetSize(), box, &format);
			warn_stat("path.AddString");

			RenderOutlineText(graphics_bitmap, path, brush);
		} else {
			stat = graphics_bitmap.DrawString(text.c_str(),
							  (int)text.size(),
							  font.get(), box,
							  &format, &brush);
			warn_stat("graphics_bitmap.DrawString");
		}
	}
	bool change = false;
	if (!tex || (LONG)cx != size.cx || (LONG)cy != size.cy) {
		obs_enter_graphics();
		if (tex)
			gs_texture_destroy(tex);

		const uint8_t *data = (uint8_t *)bits.get();
		tex = gs_texture_create(size.cx, size.cy, GS_BGRA, 1, &data,
					GS_DYNAMIC);

		obs_leave_graphics();
		change = true;

	} else if (tex) {
		obs_enter_graphics();
		gs_texture_set_image(tex, bits.get(), size.cx * 4, false);
		obs_leave_graphics();
	}
	if (change) {
		cx = (uint32_t)size.cx;
		cy = (uint32_t)size.cy;
	}
}

const char *TextSource::GetMainString(const char *str)
{
	if (!str)
		return "";
	if (!chatlog_mode || !chatlog_lines)
		return str;

	int lines = chatlog_lines;
	size_t len = strlen(str);
	if (!len)
		return str;

	const char *temp = str + len;

	while (temp != str) {
		temp--;

		if (temp[0] == '\n' && temp[1] != 0) {
			if (!--lines)
				break;
		}
	}

	return *temp == '\n' ? temp + 1 : temp;
}

void TextSource::LoadFileText()
{
	BPtr<char> file_text = os_quick_read_utf8_file(file.c_str());
	text = to_wide(GetMainString(file_text));

	if (!text.empty() && text.back() != '\n')
		text.push_back('\n');
}

inline void TextSource::Update(obs_data_t *s)
{
	const char *new_text = obs_data_get_string(s, S_TEXT);
	obs_data_t *font_obj = obs_data_get_obj(s, S_FONT);
	const char *align_str = obs_data_get_string(s, S_ALIGN);
	const char *valign_str = obs_data_get_string(s, S_VALIGN);
	uint32_t new_color = obs_data_get_uint32(s, S_COLOR);
	uint32_t new_opacity = obs_data_get_uint32(s, S_OPACITY);
	bool gradient = obs_data_get_bool(s, S_GRADIENT);
	uint32_t new_color2 = obs_data_get_uint32(s, S_GRADIENT_COLOR);
	uint32_t new_opacity2 = obs_data_get_uint32(s, S_GRADIENT_OPACITY);
	float new_grad_dir = (float)obs_data_get_double(s, S_GRADIENT_DIR);
	bool new_vertical = obs_data_get_bool(s, S_VERTICAL);
	bool new_outline = obs_data_get_bool(s, S_OUTLINE);
	uint32_t new_o_color = obs_data_get_uint32(s, S_OUTLINE_COLOR);
	uint32_t new_o_opacity = obs_data_get_uint32(s, S_OUTLINE_OPACITY);
	uint32_t new_o_size = obs_data_get_uint32(s, S_OUTLINE_SIZE);
	bool new_use_file = obs_data_get_bool(s, S_USE_FILE);
	const char *new_file = obs_data_get_string(s, S_FILE);
	bool new_chat_mode = obs_data_get_bool(s, S_CHATLOG_MODE);
	int new_chat_lines = (int)obs_data_get_int(s, S_CHATLOG_LINES);
	bool new_extents = obs_data_get_bool(s, S_EXTENTS);
	bool new_extents_wrap = obs_data_get_bool(s, S_EXTENTS_WRAP);
	uint32_t n_extents_cx = obs_data_get_uint32(s, S_EXTENTS_CX);
	uint32_t n_extents_cy = obs_data_get_uint32(s, S_EXTENTS_CY);
	int new_text_transform = (int)obs_data_get_int(s, S_TRANSFORM);

	const char *font_face = obs_data_get_string(font_obj, "face");
	int font_size = (int)obs_data_get_int(font_obj, "size");
	int64_t font_flags = obs_data_get_int(font_obj, "flags");
	bool new_bold = (font_flags & OBS_FONT_BOLD) != 0;
	bool new_italic = (font_flags & OBS_FONT_ITALIC) != 0;
	bool new_underline = (font_flags & OBS_FONT_UNDERLINE) != 0;
	bool new_strikeout = (font_flags & OBS_FONT_STRIKEOUT) != 0;

	uint32_t new_bk_color = obs_data_get_uint32(s, S_BKCOLOR);
	uint32_t new_bk_opacity = obs_data_get_uint32(s, S_BKOPACITY);

	// 阴影相关
	bool new_use_shadow = obs_data_get_bool(s, S_USE_SHADOW);
	uint32_t new_shadow_color = obs_data_get_uint32(s, S_SHADOW_COLOR);
	uint32_t new_shadow_opacity = obs_data_get_uint32(s, S_SHADOW_OPACITY);
	int new_shadow_dis = obs_data_get_int(s, S_SHADOW_DIS);
	int new_shadow_dir = obs_data_get_int(s, S_SHADOW_DIRECTION);

	int subtype = obs_data_get_int(s, "subtype");
	if (subtype == RewardText || subtype == FollowText)
		special_source = true;
	else
		special_source = false;
	/* ----------------------------- */

	wstring new_face = to_wide(font_face);

	if (new_face != face || face_size != font_size || new_bold != bold ||
	    new_italic != italic || new_underline != underline ||
	    new_strikeout != strikeout) {

		face = new_face;
		face_size = font_size;
		bold = new_bold;
		italic = new_italic;
		underline = new_underline;
		strikeout = new_strikeout;

		UpdateFont();
	}

	/* ----------------------------- */

	new_color = rgb_to_bgr(new_color);
	new_color2 = rgb_to_bgr(new_color2);
	new_o_color = rgb_to_bgr(new_o_color);
	new_bk_color = rgb_to_bgr(new_bk_color);
	new_shadow_color = rgb_to_bgr(new_shadow_color);
	color = new_color;
	opacity = new_opacity;
	color2 = new_color2;
	opacity2 = new_opacity2;
	gradient_dir = new_grad_dir;
	vertical = new_vertical;

	bk_color = new_bk_color;
	bk_opacity = new_bk_opacity;
	use_extents = new_extents;
	wrap = new_extents_wrap;
	extents_cx = n_extents_cx;
	extents_cy = n_extents_cy;
	text_transform = new_text_transform;

	if (bk_color == 0)
		bk_opacity = 0;
	else
		bk_opacity = 100;

	if (!gradient) {
		color2 = color;
		opacity2 = opacity;
	}

	read_from_file = new_use_file;

	chatlog_mode = new_chat_mode;
	chatlog_lines = new_chat_lines;

	if (read_from_file) {
		file = new_file;
		file_timestamp = get_modified_timestamp(new_file);
		LoadFileText();

	} else {
		text = to_wide(GetMainString(new_text));

		/* all text should end with newlines due to the fact that GDI+
		 * treats strings without newlines differently in terms of
		 * render size */
		if (!text.empty())
			text.push_back('\n');
	}
	if (text_transform == S_TRANSFORM_UPPERCASE)
		transform(text.begin(), text.end(), text.begin(), towupper);
	else if (text_transform == S_TRANSFORM_LOWERCASE)
		transform(text.begin(), text.end(), text.begin(), towlower);

	use_outline = new_outline;
	outline_color = new_o_color;
	outline_opacity = new_o_opacity;
	outline_size = roundf(float(new_o_size));

	use_shadow = new_use_shadow;
	shadow_color = new_shadow_color;
	shadow_opacity = new_shadow_opacity;
	shadow_distance = new_shadow_dis * 4.0f / 100.0f;
	shadow_dir = new_shadow_dir;

	if (strcmp(align_str, S_ALIGN_CENTER) == 0)
		align = Align::Center;
	else if (strcmp(align_str, S_ALIGN_RIGHT) == 0)
		align = Align::Right;
	else
		align = Align::Left;

	if (strcmp(valign_str, S_VALIGN_CENTER) == 0)
		valign = VAlign::Center;
	else if (strcmp(valign_str, S_VALIGN_BOTTOM) == 0)
		valign = VAlign::Bottom;
	else
		valign = VAlign::Top;

	RenderText();
	update_time_elapsed = 0.0f;

	/* ----------------------------- */

	obs_data_release(font_obj);
}

inline void TextSource::Tick(float seconds)
{
	if (!read_from_file)
		return;

	update_time_elapsed += seconds;

	if (update_time_elapsed >= 1.0f) {
		time_t t = get_modified_timestamp(file.c_str());
		update_time_elapsed = 0.0f;

		if (update_file) {
			LoadFileText();
			RenderText();
			update_file = false;
		}

		if (file_timestamp != t) {
			file_timestamp = t;
			update_file = true;
		}
	}
}

inline void TextSource::Render()
{
	if (!tex)
		return;

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"),
			      tex);
	gs_draw_sprite(tex, 0, cx, cy);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

/* ------------------------------------------------------------------------- */

static ULONG_PTR gdip_token = 0;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-text", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Windows GDI+ text source";
}

#define set_vis(var, val, show)                           \
	do {                                              \
		p = obs_properties_get(props, val);       \
		obs_property_set_visible(p, var == show); \
	} while (false)

static bool use_file_changed(obs_properties_t *props, obs_property_t *p,
			     obs_data_t *s)
{
	bool use_file = obs_data_get_bool(s, S_USE_FILE);

	set_vis(use_file, S_TEXT, false);
	set_vis(use_file, S_FILE, true);
	return true;
}

static bool outline_changed(obs_properties_t *props, obs_property_t *p,
			    obs_data_t *s)
{
	bool outline = obs_data_get_bool(s, S_OUTLINE);

	set_vis(outline, S_OUTLINE_SIZE, true);
	set_vis(outline, S_OUTLINE_COLOR, true);
	set_vis(outline, S_OUTLINE_OPACITY, true);
	return true;
}

static bool chatlog_mode_changed(obs_properties_t *props, obs_property_t *p,
				 obs_data_t *s)
{
	bool chatlog_mode = obs_data_get_bool(s, S_CHATLOG_MODE);

	set_vis(chatlog_mode, S_CHATLOG_LINES, true);
	return true;
}

static bool gradient_changed(obs_properties_t *props, obs_property_t *p,
			     obs_data_t *s)
{
	bool gradient = obs_data_get_bool(s, S_GRADIENT);

	set_vis(gradient, S_GRADIENT_COLOR, true);
	set_vis(gradient, S_GRADIENT_OPACITY, true);
	set_vis(gradient, S_GRADIENT_DIR, true);
	return true;
}

static bool extents_modified(obs_properties_t *props, obs_property_t *p,
			     obs_data_t *s)
{
	bool use_extents = obs_data_get_bool(s, S_EXTENTS);

	set_vis(use_extents, S_EXTENTS_WRAP, true);
	set_vis(use_extents, S_EXTENTS_CX, true);
	set_vis(use_extents, S_EXTENTS_CY, true);
	return true;
}

#undef set_vis

static obs_properties_t *get_properties(void *data)
{
	TextSource *s = reinterpret_cast<TextSource *>(data);
	string path;

	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	obs_properties_add_font(props, S_FONT, T_FONT);

	p = obs_properties_add_bool(props, S_USE_FILE, T_USE_FILE);
	obs_property_set_modified_callback(p, use_file_changed);

	string filter;
	filter += T_FILTER_TEXT_FILES;
	filter += " (*.txt);;";
	filter += T_FILTER_ALL_FILES;
	filter += " (*.*)";

	if (s && !s->file.empty()) {
		const char *slash;

		path = s->file;
		replace(path.begin(), path.end(), '\\', '/');
		slash = strrchr(path.c_str(), '/');
		if (slash)
			path.resize(slash - path.c_str() + 1);
	}

	obs_properties_add_text(props, S_TEXT, T_TEXT, OBS_TEXT_MULTILINE);
	obs_properties_add_path(props, S_FILE, T_FILE, OBS_PATH_FILE,
				filter.c_str(), path.c_str());

	p = obs_properties_add_list(props, S_TRANSFORM, T_TRANSFORM,
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T_TRANSFORM_NONE, S_TRANSFORM_NONE);
	obs_property_list_add_int(p, T_TRANSFORM_UPPERCASE,
				  S_TRANSFORM_UPPERCASE);
	obs_property_list_add_int(p, T_TRANSFORM_LOWERCASE,
				  S_TRANSFORM_LOWERCASE);

	obs_properties_add_bool(props, S_VERTICAL, T_VERTICAL);
	obs_properties_add_color(props, S_COLOR, T_COLOR);
	obs_properties_add_int_slider(props, S_OPACITY, T_OPACITY, 0, 100, 1);

	p = obs_properties_add_bool(props, S_GRADIENT, T_GRADIENT);
	obs_property_set_modified_callback(p, gradient_changed);

	obs_properties_add_color(props, S_GRADIENT_COLOR, T_GRADIENT_COLOR);
	obs_properties_add_int_slider(props, S_GRADIENT_OPACITY,
				      T_GRADIENT_OPACITY, 0, 100, 1);
	obs_properties_add_float_slider(props, S_GRADIENT_DIR, T_GRADIENT_DIR,
					0, 360, 0.1);

	obs_properties_add_color(props, S_BKCOLOR, T_BKCOLOR);
	obs_properties_add_int_slider(props, S_BKOPACITY, T_BKOPACITY, 0, 100,
				      1);

	p = obs_properties_add_list(props, S_ALIGN, T_ALIGN,
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, T_ALIGN_LEFT, S_ALIGN_LEFT);
	obs_property_list_add_string(p, T_ALIGN_CENTER, S_ALIGN_CENTER);
	obs_property_list_add_string(p, T_ALIGN_RIGHT, S_ALIGN_RIGHT);

	p = obs_properties_add_list(props, S_VALIGN, T_VALIGN,
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, T_VALIGN_TOP, S_VALIGN_TOP);
	obs_property_list_add_string(p, T_VALIGN_CENTER, S_VALIGN_CENTER);
	obs_property_list_add_string(p, T_VALIGN_BOTTOM, S_VALIGN_BOTTOM);

	p = obs_properties_add_bool(props, S_OUTLINE, T_OUTLINE);
	obs_property_set_modified_callback(p, outline_changed);

	obs_properties_add_int(props, S_OUTLINE_SIZE, T_OUTLINE_SIZE, 1, 20, 1);
	obs_properties_add_color(props, S_OUTLINE_COLOR, T_OUTLINE_COLOR);
	obs_properties_add_int_slider(props, S_OUTLINE_OPACITY,
				      T_OUTLINE_OPACITY, 0, 100, 1);

	p = obs_properties_add_bool(props, S_CHATLOG_MODE, T_CHATLOG_MODE);
	obs_property_set_modified_callback(p, chatlog_mode_changed);

	obs_properties_add_int(props, S_CHATLOG_LINES, T_CHATLOG_LINES, 1, 1000,
			       1);

	p = obs_properties_add_bool(props, S_EXTENTS, T_EXTENTS);
	obs_property_set_modified_callback(p, extents_modified);

	obs_properties_add_int(props, S_EXTENTS_CX, T_EXTENTS_CX, 32, 8000, 1);
	obs_properties_add_int(props, S_EXTENTS_CY, T_EXTENTS_CY, 32, 8000, 1);
	obs_properties_add_bool(props, S_EXTENTS_WRAP, T_EXTENTS_WRAP);

	return props;
}

extern struct obs_source_info slide_text = {};
extern struct obs_source_info scroll_text = {};
bool obs_module_load(void)
{
	obs_source_info si = {};
	si.id = "text_gdiplus";
	si.type = OBS_SOURCE_TYPE_INPUT;
	si.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	si.get_properties = get_properties;

	si.get_name = [](void *) { return obs_module_text("TextGDIPlus"); };
	si.create = [](obs_data_t *settings, obs_source_t *source) {
		return (void *)new TextSource(source, settings);
	};
	si.destroy = [](void *data) {
		delete reinterpret_cast<TextSource *>(data);
	};
	si.get_width = [](void *data) {
		return reinterpret_cast<TextSource *>(data)->cx;
	};
	si.get_height = [](void *data) {
		return reinterpret_cast<TextSource *>(data)->cy;
	};
	si.get_defaults = [](obs_data_t *settings) {
		obs_data_t *font_obj = obs_data_create();
		obs_data_set_default_string(font_obj, "face", "Arial");
		obs_data_set_default_int(font_obj, "size", 36);

		obs_data_set_default_obj(settings, S_FONT, font_obj);
		obs_data_set_default_string(settings, S_ALIGN, S_ALIGN_LEFT);
		obs_data_set_default_string(settings, S_VALIGN, S_VALIGN_TOP);
		obs_data_set_default_int(settings, S_COLOR, 0xFFFFFF);
		obs_data_set_default_int(settings, S_OPACITY, 100);
		obs_data_set_default_int(settings, S_GRADIENT_COLOR, 0xFFFFFF);
		obs_data_set_default_int(settings, S_GRADIENT_OPACITY, 100);
		obs_data_set_default_double(settings, S_GRADIENT_DIR, 90.0);
		obs_data_set_default_int(settings, S_BKCOLOR, 0x000000);
		obs_data_set_default_int(settings, S_BKOPACITY, 0);
		obs_data_set_default_int(settings, S_OUTLINE_SIZE, 1);
		obs_data_set_default_int(settings, S_OUTLINE_COLOR, 0xFFFFFF);
		obs_data_set_default_int(settings, S_OUTLINE_OPACITY, 100);
		obs_data_set_default_int(settings, S_CHATLOG_LINES, 6);
		obs_data_set_default_bool(settings, S_EXTENTS_WRAP, true);
		obs_data_set_default_int(settings, S_EXTENTS_CX, 100);
		obs_data_set_default_int(settings, S_EXTENTS_CY, 100);
		obs_data_set_default_int(settings, S_TRANSFORM,
					 S_TRANSFORM_NONE);
		obs_data_set_default_bool(settings, S_USE_SHADOW, false);
		obs_data_set_default_int(settings, S_SHADOW_COLOR, 0x000000);
		obs_data_set_default_int(settings, S_SHADOW_OPACITY, 50);
		obs_data_set_default_int(settings, S_SHADOW_DIRECTION, 2);
		obs_data_set_default_int(settings, S_SHADOW_DIS, 50);
		obs_data_release(font_obj);
	};
	si.update = [](void *data, obs_data_t *settings) {
		reinterpret_cast<TextSource *>(data)->Update(settings);
	};
	si.video_tick = [](void *data, float seconds) {
		reinterpret_cast<TextSource *>(data)->Tick(seconds);
	};
	si.video_render = [](void *data, gs_effect_t *) {
		reinterpret_cast<TextSource *>(data)->Render();
	};

	obs_register_source(&si);

	/////////slide_text////////////////////////////////////
	//obs_source_info slide_text = {};
	slide_text.id = "quicktextslideshow_source";
	slide_text.type = OBS_SOURCE_TYPE_INPUT;
	slide_text.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	slide_text.get_properties = get_properties;

	slide_text.get_name = [](void *) {
		return obs_module_text("SlideTextGDIPlus");
	};
	slide_text.create = [](obs_data_t *settings, obs_source_t *source) {
		return (void *)new SlideTextSource(source, settings);
	};
	slide_text.destroy = [](void *data) {
		delete reinterpret_cast<SlideTextSource *>(data);
	};
	slide_text.get_width = [](void *data) {
		return reinterpret_cast<SlideTextSource *>(data)->cx;
	};
	slide_text.get_height = [](void *data) {
		return reinterpret_cast<SlideTextSource *>(data)->cy;
	};
	slide_text.get_defaults = [](obs_data_t *settings) {
		/////////////////////////////////////////////////////////
		obs_data_set_default_int(settings, "speed", 1);
		obs_data_set_default_int(settings, "direction", 3);
		obs_data_set_default_string(settings, "fillColor", "#00000000");
		obs_data_set_default_string(settings, "frontColor", "#FFFFFF");
		obs_data_set_default_int(settings, "horizontalAlignment",
					 0x0001);
		obs_data_set_default_int(settings, "verticalAlignment", 0x0020);
		obs_data_set_default_int(settings, "fontSize", 36);
		obs_data_set_default_bool(settings, "hDisplay", true);
		obs_data_set_default_string(settings, "font", u8"微软雅黑");
	};
	slide_text.update = [](void *data, obs_data_t *settings) {
		reinterpret_cast<SlideTextSource *>(data)->SlideUpdate(
			settings);
	};
	slide_text.video_tick = [](void *data, float seconds) {
		reinterpret_cast<SlideTextSource *>(data)->SlideTick(seconds);
	};
	slide_text.video_render = [](void *data, gs_effect_t *) {
		reinterpret_cast<SlideTextSource *>(data)->SlideRender();
	};
	slide_text.make_command = [](void *data, obs_data_t *command) {
		reinterpret_cast<SlideTextSource *>(data)
			->SlideTextCustomCommand(command);
	};

	obs_register_source(&slide_text);

	/////////scroll_text////////////////////////////////////
	scroll_text.id = "quicktextscrollshow_source";
	scroll_text.type = OBS_SOURCE_TYPE_INPUT;
	scroll_text.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	scroll_text.get_properties = get_properties;
	scroll_text.get_name = [](void *) {
		return obs_module_text("ScrollTextGDIPlus");
	};
	scroll_text.create = [](obs_data_t *settings, obs_source_t *source) {
		return (void *)new ScrollTextSource(source, settings);
	};
	scroll_text.destroy = [](void *data) {
		delete reinterpret_cast<ScrollTextSource *>(data);
	};
	scroll_text.get_width = [](void *data) {
		return reinterpret_cast<ScrollTextSource *>(data)->cx;
	};
	scroll_text.get_height = [](void *data) {
		return reinterpret_cast<ScrollTextSource *>(data)->cy;
	};
	scroll_text.get_defaults = [](obs_data_t *settings) {
		/////////////////////////////////////////////////////////
		obs_data_set_default_int(settings, "speed", 1);
		obs_data_set_default_int(settings, "direction", 1);
		obs_data_set_default_string(settings, "fillColor", "#00000000");
		obs_data_set_default_string(settings, "frontColor", "#FFFFFF");
		obs_data_set_default_int(settings, "horizontalAlignment",
					 0x0001);
		obs_data_set_default_int(settings, "verticalAlignment", 0x0020);
		obs_data_set_default_int(settings, "fontSize", 36);
		obs_data_set_default_bool(settings, "hDisplay", true);
		obs_data_set_default_string(settings, "font", u8"微软雅黑");
	};
	scroll_text.update = [](void *data, obs_data_t *settings) {
		reinterpret_cast<ScrollTextSource *>(data)->ScrollUpdate(
			settings);
	};
	scroll_text.video_tick = [](void *data, float seconds) {
		reinterpret_cast<ScrollTextSource *>(data)->ScrollTick(seconds);
	};
	scroll_text.video_render = [](void *data, gs_effect_t *) {
		reinterpret_cast<ScrollTextSource *>(data)->ScrollRender();
	};

	scroll_text.make_command = [](void *data, obs_data_t *command) {
		reinterpret_cast<ScrollTextSource *>(data)
			->ScrollTextCustomCommand(command);
	};
	obs_register_source(&scroll_text);

	const GdiplusStartupInput gdip_input;
	GdiplusStartup(&gdip_token, &gdip_input, nullptr);
	return true;
}

void obs_module_unload(void)
{
	GdiplusShutdown(gdip_token);
}
