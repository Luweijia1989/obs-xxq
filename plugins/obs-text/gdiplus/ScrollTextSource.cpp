#include "ScrollTextSource.h" animate_time =

void ScrollTextSource::ScrollUpdate(obs_data_t *s)
{
	const char *new_text = obs_data_get_string(s, S_TEXT);
	uint32_t new_halign = obs_data_get_int(s, S_HORALIGN);
	uint32_t new_valign = obs_data_get_int(s, S_VERALIGN);
	bool new_display = obs_data_get_bool(s, S_DISPLAY);
	uint32_t new_speed = obs_data_get_int(s, S_SPEED);
	uint32_t new_direction = obs_data_get_int(s, S_DIRECTION);
	bool new_bold = obs_data_get_bool(s, S_BOLD);
	bool new_italic = obs_data_get_bool(s, S_ITALIC);
	bool new_strikeout = obs_data_get_bool(s, S_STRIKEOUT);
	bool new_underline = obs_data_get_bool(s, S_UNDERLINE);
	const char *font_face = obs_data_get_string(s, S_FONT);
	int new_fontSize = obs_data_get_int(s, S_FONTSIZE);
	const char *new_color = obs_data_get_string(s, S_FILLCOLOR);
	uint32_t new_color2 = obs_data_get_uint32(s, S_GRADIENT_COLOR);
	float new_grad_dir = (float)obs_data_get_double(s, S_GRADIENT_DIR);
	bool gradient = obs_data_get_bool(s, S_GRADIENT);
	const char *new_front_color = obs_data_get_string(s, S_FONTCOLOR);
	bool new_vertical = obs_data_get_bool(s, S_DISPLAY) == false ? true
								     : false;
	uint32_t new_bk_color = obs_data_get_uint32(s, S_BKCOLOR);

	//描边
	bool new_outline = obs_data_get_bool(s, S_OUTLINE);
	uint32_t new_o_color = obs_data_get_uint32(s, S_OUTLINE_COLOR);
	uint32_t new_o_opacity = obs_data_get_uint32(s, S_OUTLINE_OPACITY);
	uint32_t new_o_size = obs_data_get_uint32(s, S_OUTLINE_SIZE);

	if (strcmp(new_color, "#FFF244F") == 0) {
		new_color = "#00000000";
	}

	if (strcmp(new_front_color, "#FFF244F") == 0) {
		new_front_color = "#00000000";
	}
	vertical = new_vertical;
	direction = new_direction;
	speed = new_speed;
	display = new_display;

	/* ----------------------------- */
	wstring new_face = to_wide(font_face);
	face = new_face;
	face_size = new_fontSize;
	bold = new_bold;
	italic = new_italic;
	underline = new_underline;
	strikeout = new_strikeout;
	UpdateFont();
	/* ----------------------------- */
	string str_color = new_front_color;
	str_color = str_color.substr(1);
	str_color = "0x" + str_color;

	sscanf(str_color.c_str(), "%x", &color); //十六进制转数字
	str_color = new_color;
	str_color = str_color.substr(1);
	str_color = "0x" + str_color;
	CaclculateSpeed();

	if (new_halign == 0x0004)
		align = Align::Center;
	else if (new_halign == 0x0002)
		align = Align::Right;
	else
		align = Align::Left;

	if (new_valign == 0x0080)
		valign = VAlign::Center;
	else if (new_valign == 0x0040)
		valign = VAlign::Bottom;
	else
		valign = VAlign::Top;

	text = to_wide(new_text);
	update_time_elapsed = 0.0f;
	/* ----------------------------- */
	update_time1 = 0.0002f;
	update_time2 = 0.0000f;
	has_twotext = false;

	use_outline = new_outline;
	outline_color = rgb_to_bgr(new_o_color);
	outline_opacity = new_o_opacity;
	outline_size = roundf(float(new_o_size));
	color2 = rgb_to_bgr(new_color2);
	bk_color = rgb_to_bgr(new_bk_color);
	if (bk_color == 0)
		bk_opacity = 0;
	else
		bk_opacity = 100;
	gradient_dir = new_grad_dir;
	if (!gradient) {
		color2 = color;
		opacity2 = opacity;
	} else {
		uint32_t color_tmp = obs_data_get_uint32(s, S_COLOR);
		color = rgb_to_bgr(color_tmp);
	}
	RenderText();
}

void ScrollTextSource::RenderText()
{
	StringFormat format(StringFormat::GenericTypographic());
	Status stat;

	RectF box;
	SIZE size;

	GetSlideStringFormat(format);
	CalculateTextSizes(text, format, box, size);
	float recbox_with = box.Width;
	unique_ptr<uint8_t> bits(new uint8_t[size.cx * size.cy * 4]);
	Bitmap bitmap(size.cx, size.cy, 4 * size.cx, PixelFormat32bppARGB,
		      bits.get());

	Graphics graphics_bitmap(&bitmap);
	DWORD full_bk_color = bk_color & 0xFFFFFF;

	if (!text.empty() || use_extents)
		full_bk_color |= get_alpha_val(bk_opacity);

	graphics_bitmap.SetTextRenderingHint(TextRenderingHintAntiAlias);
	graphics_bitmap.SetCompositingMode(CompositingModeSourceOver);
	graphics_bitmap.SetSmoothingMode(SmoothingModeAntiAlias);

	stat = graphics_bitmap.Clear(Color(0));
	if ((size.cx > box.Width || size.cy > box.Height) && !use_extents) {
		warn_stat("graphics_bitmap.Clear");
		SolidBrush bk_brush = Color(full_bk_color);
		RectF box_brush;
		box_brush.X = 0.0f;
		box_brush.Y = 0.0f;
		box_brush.Width = size.cx;
		box_brush.Height = size.cy;
		stat = graphics_bitmap.FillRectangle(&bk_brush, box_brush);
		warn_stat("graphics_bitmap.FillRectangle");
	} else {
		stat = graphics_bitmap.Clear(Color(full_bk_color));
		warn_stat("graphics_bitmap.Clear");
	}

	if (use_outline) {
		box.Offset(outline_size / 2, outline_size / 2);
		int count = CaculateTextColums(text);

		if ((update_time1 < animate_time && update_time1 > 0.0f)) {
			GraphicsPath path1;
			float posx = 0;
			float posy = 0;
			CalculateTextPos(count, format, posx, posy, size,
					 update_time1);
			box.X = posx;
			box.Y = posy;
			LinearGradientBrush brush1(
				box, Color(calc_color(color, opacity)),
				Color(calc_color(color2, opacity2)),
				gradient_dir, 1);
			stat = path1.AddString(text.c_str(), (int)text.size(),
					       &families[0], font->GetStyle(),
					       font->GetSize(), box, &format);
			warn_stat("path.AddString");
			RenderSlideOutlineText(graphics_bitmap, path1, brush1);
		}

		if ((update_time2 < animate_time && update_time2 > 0.0f)) {
			GraphicsPath path2;
			float posx = 0;
			float posy = 0;
			CalculateTextPos(count, format, posx, posy, size,
					 update_time2);
			box.X = posx;
			box.Y = posy;
			LinearGradientBrush brush2(
				box, Color(calc_color(color, opacity)),
				Color(calc_color(color2, opacity2)),
				gradient_dir, 1);

			stat = path2.AddString(text.c_str(), (int)text.size(),
					       &families[0], font->GetStyle(),
					       font->GetSize(), box, &format);
			warn_stat("path.AddString");
			RenderSlideOutlineText(graphics_bitmap, path2, brush2);
		}
	} else {
		int count = CaculateTextColums(text);

		if ((update_time1 < animate_time && update_time1 > 0.0f)) {
			float posx = 0;
			float posy = 0;
			CalculateTextPos(count, format, posx, posy, size,
					 update_time1);
			box.X = posx;
			box.Y = posy;
			LinearGradientBrush brush1(
				box, Color(calc_color(color, opacity)),
				Color(calc_color(color2, opacity2)),
				gradient_dir, 1);
			stat = graphics_bitmap.DrawString(text.c_str(),
							  (int)text.size(),
							  font.get(), box,
							  &format, &brush1);
		}

		if ((update_time2 < animate_time && update_time2 > 0.0f)) {
			float posx = 0;
			float posy = 0;
			CalculateTextPos(count, format, posx, posy, size,
					 update_time2);
			box.X = posx;
			box.Y = posy;
			LinearGradientBrush brush2(
				box, Color(calc_color(color, opacity)),
				Color(calc_color(color2, opacity2)),
				gradient_dir, 1);
			stat = graphics_bitmap.DrawString(text.c_str(),
							  (int)text.size(),
							  font.get(), box,
							  &format, &brush2);
		}
		warn_stat("graphics_bitmap.DrawString");
	}

	if (!tex || (LONG)cx != size.cx || (LONG)cy != size.cy) {
		obs_enter_graphics();
		if (tex)
			gs_texture_destroy(tex);

		const uint8_t *data = (uint8_t *)bits.get();
		tex = gs_texture_create(size.cx, size.cy, GS_BGRA, 1, &data,
					GS_DYNAMIC);

		obs_leave_graphics();

		cx = (uint32_t)size.cx;
		cy = (uint32_t)size.cy;

	} else if (tex) {
		obs_enter_graphics();
		gs_texture_set_image(tex, bits.get(), size.cx * 4, false);
		obs_leave_graphics();
	}
}

void ScrollTextSource::ScrollTick(float seconds)
{
	if (text.length() > 0) {
		bool change = false;
		if ((update_time1 < animate_time && update_time1 > 0.0001f)) {
			update_time1 = update_time1 + seconds;
			change = true;
			if (update_time1 >= (animate_time * 2 / 3)) {
				if (!has_twotext) {
					has_twotext = true;
					update_time2 = 0.0002f;
				}
			}

		} else if (update_time1 > animate_time) {
			change = true;
			has_twotext = false;
			update_time1 = 0.0f;
		}

		if ((update_time2 < animate_time && update_time2 > 0.0001f)) {
			change = true;
			update_time2 = update_time2 + seconds;
			if (update_time2 >= (animate_time * 2 / 3)) {
				if (!has_twotext) {
					has_twotext = true;
					update_time1 = 0.0002f;
				}
			}
		} else if (update_time2 > animate_time) {
			change = true;
			has_twotext = false;
			update_time2 = 0.0f;
		}

		if (change)
			RenderText();

		if (update_time1 < 0.0001f && update_time2 < 0.0001f)
			update_time1 = 0.0002f;
	}
}

void ScrollTextSource::UpdateFont()
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

void ScrollTextSource::GetSlideStringFormat(StringFormat &format)
{
	UINT flags = StringFormatFlagsNoFitBlackBox |
		     StringFormatFlagsMeasureTrailingSpaces;

	format.SetFormatFlags(flags);
	format.SetTrimming(StringTrimmingWord);

	if (!vertical) {
		switch (align) {
		case Align::Left:
			format.SetAlignment(StringAlignmentNear);
			break;
		case Align::Center:
			format.SetAlignment(StringAlignmentCenter);
			break;
		case Align::Right:
			format.SetAlignment(StringAlignmentFar);
			break;
		}
	}

	else {
		switch (valign) {
		case VAlign::Top:
			format.SetLineAlignment(StringAlignmentNear);
			break;
		case VAlign::Center:
			format.SetLineAlignment(StringAlignmentCenter);
			break;
		case VAlign::Bottom:
			format.SetLineAlignment(StringAlignmentFar);
			break;
		}
	}
}

/* GDI+ treats '\n' as an extra character with an actual render size when
 * calculating the texture size, so we have to calculate the size of '\n' to
 * remove the padding.  Because we always add a newline to the string, we
 * also remove the extra unused newline. */
void ScrollTextSource::RemoveSlideNewlinePadding(const StringFormat &format,
						 RectF &box)
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

int ScrollTextSource::CaculateTextColums(const wstring &text_measure)
{
	wstring strtmp = text_measure;
	int index = strtmp.find_first_of(L'\n');
	int column_count = 0;
	while (index != -1) {
		column_count++;
		strtmp = strtmp.substr(index + 1);
		index = strtmp.find_first_of(L'\n');
	}
	return column_count;
}

void ScrollTextSource::CalculateTextPos(int count, const StringFormat &format,
					float &posx, float &posy,
					const SIZE &size, const float &posTime)
{
	RectF box;
	graphics.MeasureString(L"字", 2, font.get(), PointF(0.0f, 0.0f),
			       &format, &box);
	int font_height = box.Height;
	int font_width = box.Width;
	if (valign == VAlign::Center)
		posy = size.cy * 0.5f - font_height * (count + 0.5f) * 0.5f;
	else if (valign == VAlign::Bottom)
		posy = size.cy - font_height * (count + 1);

	float verPos = 0.0f;
	float horPos = 0.0f;
	if (vertical) {
		verPos = max_size.cy;
		horPos = font_width;
	} else {
		SIZE size;
		CalculateTextSizes(text, format, box, size);
		verPos = size.cy;
		horPos = size.cx;
	}

	switch (direction) {
	case BottomToTop: {
		posy = size.cy - 2 * (size.cy - posy) * posTime / animate_time;
	} break;
	case TopToBottom: {
		posy = -verPos + 2 * (posy + verPos) * posTime / animate_time;
	} break;
	case RightToLeft: {
		posx = size.cx - 2 * (size.cx - posx) * posTime / animate_time;
	} break;
	case LeftToRight: {
		posx = -size.cx + 2 * (posx + size.cx) * posTime / animate_time;
	} break;
	default:
		break;
	}
}

void ScrollTextSource::CalculateMaxSize()
{
	if (texts.size() == 0)
		return;
	int count = texts.size();
	max_size.cx = 0;
	max_size.cy = 0;
	StringFormat format(StringFormat::GenericTypographic());
	RectF box;
	SIZE size;
	GetSlideStringFormat(format);

	for (int i = 0; i < count; i++) {
		if (!vertical) {
			string tmp = texts.at(i);
			if (!tmp.empty()) {
				CalculateTextSizes(to_wide(tmp.c_str()), format,
						   box, size);

				if (box.Width > max_size.cx)
					max_size.cx = box.Width;

				if (box.Height > max_size.cy)
					max_size.cy = box.Height;
			}
		} else {
			string tmp = texts.at(i);
			if (!tmp.empty()) {
				std::vector<wstring> texts_vec;
				GenerateVerSlideText(to_wide(tmp.c_str()),
						     texts_vec);
				for (auto str : texts_vec) {
					CalculateTextSizes(str, format, box,
							   size);

					if (size.cx > max_size.cx)
						max_size.cx = size.cx;

					if (size.cy > max_size.cy)
						max_size.cy = size.cy;
				}
			}
		}
	}
}

void ScrollTextSource::GenerateVerSlideText(const wstring &text_tansforms,
					    std::vector<wstring> &texts_vec)
{
	wstring text_tmp = text_tansforms;
	if (!text_tmp.empty())
		text_tmp.push_back('\n');

	texts_vec.clear();
	wstring strtmp1 = text_tmp;
	int index = strtmp1.find_first_of(L'\n');
	while (index != -1) {
		wstring strpre = strtmp1.substr(0, index);
		texts_vec.push_back(strpre);
		strtmp1 = strtmp1.substr(index + 1);
		index = strtmp1.find_first_of(L'\n');
	}

	for (auto &str : texts_vec) {
		wstring strtmp2;
		for (int i = 0; i < str.size(); i++) {
			strtmp2.push_back(str.at(i));
			strtmp2.push_back(L'\n');
		}
		str = strtmp2;
	}
}

void ScrollTextSource::CalculateTextSizes(const wstring &text_measure,
					  const StringFormat &format,
					  RectF &bounding_box, SIZE &text_size)
{
	RectF layout_box;
	RectF temp_box;
	Status stat;

	if (!text_measure.empty()) {
		if (use_extents && wrap) {
			layout_box.X = layout_box.Y = 0;
			layout_box.Width = float(extents_cx);
			layout_box.Height = float(extents_cy);

			if (use_outline) {
				layout_box.Width -= outline_size;
				layout_box.Height -= outline_size;
			}

			stat = graphics.MeasureString(
				text_measure.c_str(),
				(int)text_measure.size() + 1, font.get(),
				layout_box, &format, &bounding_box);
			warn_stat("MeasureString (wrapped)");

			temp_box = bounding_box;
		} else {
			stat = graphics.MeasureString(
				text_measure.c_str(),
				(int)text_measure.size() + 1, font.get(),
				PointF(0.0f, 0.0f), &format, &bounding_box);
			warn_stat("MeasureString (non-wrapped)");

			temp_box = bounding_box;

			bounding_box.X = 0.0f;
			bounding_box.Y = 0.0f;

			RemoveSlideNewlinePadding(format, bounding_box);
			bounding_box.Width += face_size;
			bounding_box.Height += face_size;
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
}

void ScrollTextSource::RenderSlideOutlineText(Graphics &graphics,
					      const GraphicsPath &path,
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

bool ScrollTextSource::NeedRender()
{
	bool render = true;
	if (text.size() == 0)
		render = false;
	return render;
}

const char *ScrollTextSource::GetMainSlideString(const char *str)
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

void ScrollTextSource::ScrollRender()
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

void ScrollTextSource::CaclculateSpeed()
{
	switch (speed) {
	case 1:
		animate_time = 13.0f;
		break;
	case 2:
		animate_time = 10.0f;
		break;
	case 3:
		animate_time = 7.0f;
		break;
	default:
		break;
	}
}

bool ScrollTextSource::VerDeleteLineLarge()
{
	StringFormat format(StringFormat::GenericTypographic());
	Status stat;

	RectF box;
	SIZE size;

	GetSlideStringFormat(format);

	graphics.MeasureString(L"1", 2, font.get(), box, &format, &box);

	if (box.Width > face_size / 2.0f)
		return true;
	else
		return false;
}

void ScrollTextSource::ScrollTextCustomCommand(obs_data_t *cmd)
{
	const char *cmdType = obs_data_get_string(cmd, "type");
	if (strcmp("replay", cmdType) == 0) {
		update_time1 = 0.0002f;
		update_time2 = 0.0000f;
		has_twotext = false;
	}
}
