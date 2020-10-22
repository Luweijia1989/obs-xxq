#include "SlideTextSource.h"

bool SlideTextSource::IsSlideTextInstallFont(const wchar_t *fontName)
{
	bool bRtn = false;
	InstalledFontCollection *fonts = new InstalledFontCollection;
	INT found;
	int count = fonts->GetFamilyCount();
	FontFamily *fontFamily = new FontFamily[count];
	fonts->GetFamilies(count, fontFamily, &found);
	for (int i = 0; i < fonts->GetFamilyCount(); i++) {
		WCHAR wInstallFaceName[LF_FACESIZE];
		fontFamily[i].GetFamilyName(wInstallFaceName);
		if (wcscmp(fontName, wInstallFaceName) == 0) {
			bRtn = true;
			break;
		}
	}
	delete fonts;
	fonts = nullptr;
	delete[] fontFamily;
	fontFamily = NULL;
	return bRtn;
}

void SlideTextSource::UpdateSlideFont()
{
	hfont = nullptr;
	font.reset(nullptr);
	if (face == L"阿里汉仪智能黑体" || face == L"DIN Condensed" ||
	    face == L"阿里巴巴普惠体 R" || face == L"阿里巴巴普惠体 M") {
		bool bInstall = IsSlideTextInstallFont(face.c_str());
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

	if (hfont)
		font.reset(new Font(hdc, hfont));
}

wstring SlideTextSource::FontPath(const wchar_t *fontName)
{
	wstring path = L"";
	if (wcscmp(fontName, L"DIN Condensed") == 0)
		path = L"\\resource\\font\\DIN Condensed Bold.ttf";
	else if (wcscmp(fontName, L"阿里汉仪智能黑体") == 0)
		path = L"\\resource\\font\\ALiHanYiZhiNengHeiTi-2.ttf";
	else if (wcscmp(fontName, L"阿里巴巴普惠体 R") == 0)
		path = L"\\resource\\font\\Alibaba-PuHuiTi-Regular.ttf";
	else if (wcscmp(fontName, L"阿里巴巴普惠体 M") == 0)
		path = L"\\resource\\font\\Alibaba-PuHuiTi-Medium.ttf";
	return path;
}

void SlideTextSource::GetSlideStringFormat(StringFormat &format)
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
void SlideTextSource::RemoveSlideNewlinePadding(const StringFormat &format,
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

int SlideTextSource::CaculateSlideTextColums(const wstring &text_measure)
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

void SlideTextSource::CalculateSlideTextPos(int count,
					    const StringFormat &format,
					    float &posx, float &posy,
					    const SIZE &size)
{
	RectF box;
	graphics.MeasureString(L"字", 2, font.get(), PointF(0.0f, 0.0f),
			       &format, &box);
	int font_height = box.Height;
	int font_width = box.Width;

	if (!vertical) {
		if (valign == VAlign::Center)
			posy = size.cy * 0.5f -
			       font_height * (count + 0.5f) * 0.5f;
		else if (valign == VAlign::Bottom)
			posy = size.cy - font_height * (count + 1);
	} else {
		if (align == Align::Center)
			posx = size.cx * 0.5f -
			       font_width * (count + 1.0f) * 0.5f;
		else if (align == Align::Right)
			posx = size.cx - font_width * count;

		if (valign == VAlign::Bottom)
			posy = posy - font_width;
	}

	float mintime = 0.15f;
	float maxtime = animate_time - mintime;

	if (update_time < maxtime && update_time >= mintime)
		return;

	if (texts.size() == 1)
		return;
	float verPos = 0.0f;
	float horPos = 0.0f;
	if (vertical) {
		verPos = max_size.cy;
		horPos = font_width * ver_texts.size();
	} else {
		SIZE size;
		CalculateSlideTextSizes(text, format, box, size);
		verPos = size.cy;
		horPos = size.cx;
	}

	switch (direction) {
	case BottomToTop: {
		if (update_time >= maxtime)
			posy = posy - (verPos + posy) *
					      (update_time - maxtime) / mintime;
		else
			posy = size.cy -
			       (size.cy - posy) * update_time / mintime;
	} break;
	case TopToBottom: {
		if (update_time >= maxtime)
			posy = posy + (size.cy - posy) *
					      (update_time - maxtime) / mintime;
		else
			posy = -verPos +
			       (posy + verPos) * update_time / mintime;

	} break;
	case RightToLeft: {
		if (update_time >= maxtime)
			posx = posx - (posx + horPos) *
					      (update_time - maxtime) / mintime;
		else
			posx = size.cx -
			       (size.cx - posx) * update_time / mintime;

	} break;
	case LeftToRight: {
		if (update_time >= maxtime)
			posx = posx + (size.cx - posx) *
					      (update_time - maxtime) / mintime;
		else {
			if (!vertical)
				posx = -size.cx +
				       (posx + size.cx) * update_time / mintime;
			else
				posx = posx * update_time / mintime;
		}

	} break;
	default:
		break;
	}
}

void SlideTextSource::CalculateMaxSize()
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
				CalculateSlideTextSizes(to_wide(tmp.c_str()),
							format, box, size);

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
					CalculateSlideTextSizes(str, format,
								box, size);

					if (size.cx > max_size.cx)
						max_size.cx = size.cx;

					if (size.cy > max_size.cy)
						max_size.cy = size.cy;
				}
			}
		}
	}
}

void SlideTextSource::GenerateVerSlideText(const wstring &text_tansforms,
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

void SlideTextSource::CalculateSlideTextSizes(const wstring &text_measure,
					      const StringFormat &format,
					      RectF &bounding_box,
					      SIZE &text_size)
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

void SlideTextSource::RenderSlideOutlineText(Graphics &graphics,
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

bool SlideTextSource::NeedRender()
{
	bool render = true;
	if (vertical) {
		if (ver_texts.size() == 0)
			render = false;
	} else {
		if (text.size() == 0)
			render = false;
	}
	return render;
}

void SlideTextSource::RenderSlideText()
{
	StringFormat format(StringFormat::GenericTypographic());
	Status stat;

	RectF box;
	SIZE size;

	GetSlideStringFormat(format);
	CalculateSlideTextSizes(text, format, box, size);
	float recbox_with = box.Width;
	if (vertical)
		size.cx = 380;
	else
		size.cy = 380;

	if (max_size.cx > size.cx) {
		long offsetx = max_size.cx - size.cx;
		size.cx = max_size.cx;
		box.Width += offsetx;
	} else {
		max_size.cx = size.cx;
	}

	if (max_size.cy > size.cy) {
		long offsety = max_size.cy - size.cy;
		size.cy = max_size.cy;
		box.Height += offsety;
	} else {
		max_size.cy = size.cy;
	}

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

	graphics_bitmap.SetTextRenderingHint(TextRenderingHintAntiAlias);
	graphics_bitmap.SetCompositingMode(CompositingModeSourceOver);
	graphics_bitmap.SetSmoothingMode(SmoothingModeAntiAlias);

	if (use_outline) {
		box.Offset(outline_size / 2, outline_size / 2);

		FontFamily family;
		GraphicsPath path;

		font->GetFamily(&family);
		stat = path.AddString(text.c_str(), (int)text.size(), &family,
				      font->GetStyle(), font->GetSize(), box,
				      &format);
		warn_stat("path.AddString");

		RenderSlideOutlineText(graphics_bitmap, path, brush);
	} else {
		if (!vertical) {
			int count = CaculateSlideTextColums(text);
			CalculateSlideTextPos(count, format, box.X, box.Y,
					      size);

			if ((size.cx > box.Width || size.cy > box.Height) &&
			    !use_extents && fill) {
				stat = graphics_bitmap.Clear(Color(0));
				warn_stat("graphics_bitmap.Clear");
				SolidBrush bk_brush = Color(full_bk_color);
				stat = graphics_bitmap.FillRectangle(&bk_brush,
								     box);
				warn_stat("graphics_bitmap.FillRectangle");
			} else {
				stat = graphics_bitmap.Clear(
					Color(full_bk_color));
				warn_stat("graphics_bitmap.Clear");
			}

			if (fill == false) {
				stat = graphics_bitmap.Clear(Color(0));
			}
			stat = graphics_bitmap.DrawString(text.c_str(),
							  (int)text.size(),
							  font.get(), box,
							  &format, &brush);
			warn_stat("graphics_bitmap.DrawString");
		} else {
			bool b = VerDeleteLineLarge();
			RectF box1;
			format.SetAlignment(StringAlignmentCenter);
			graphics.MeasureString(L"字", 2, font.get(),
					       PointF(0.0f, 0.0f), &format,
					       &box1);
			int font_width = box1.Width;
			box.Width = font_width * 2;
			int count = ver_texts.size();
			CalculateSlideTextPos(count, format, box.X, box.Y,
					      size);

			if ((size.cx > box.Width || size.cy > box.Height) &&
			    !use_extents) {
				stat = graphics_bitmap.Clear(Color(0));
				warn_stat("graphics_bitmap.Clear");
				SolidBrush bk_brush = Color(full_bk_color);
				RectF box_rect;
				box_rect.X = box.X;
				if (b)
					box_rect.X += font_width / 4.0f;
				box_rect.Y = box.Y;
				box_rect.Width = font_width * count;
				box_rect.Height = box.Height;
				stat = graphics_bitmap.FillRectangle(&bk_brush,
								     box_rect);
				warn_stat("graphics_bitmap.FillRectangle");
			} else {
				stat = graphics_bitmap.Clear(
					Color(full_bk_color));
				warn_stat("graphics_bitmap.Clear");
			}
			float start = box.X;
			for (int i = 0; i < ver_texts.size(); i++) {
				box.X = start + i * font_width;
				text = ver_texts.at(i);

				stat = graphics_bitmap.DrawString(
					text.c_str(), (int)text.size(),
					font.get(), box, &format, &brush);

				if (strikeout) {
					RectF box1;
					SIZE size1;
					CalculateSlideTextSizes(text, format,
								box1, size1);
					DWORD rgba = calc_color(color, opacity);
					Pen pen(Color(rgba), 2);
					REAL x1, x2, y1, y2;
					if (vertical) {
						REAL offsety =
							(valign == VAlign::Top)
								? 0.0f
								: (font_width /
								   2.0f);
						if (face == L"SimSun-ExtB")
							x1 = box.X +
							     font_width * 2.0f /
								     3.0f;
						else {
							if (b)
								x1 = box.X +
								     font_width *
									     5.0f /
									     6.0f;
							else
								x1 = box.X +
								     font_width /
									     2.0f;
						}

						y1 = box.Y + offsety;
						x2 = x1;
						y2 = y1 + size1.cy;
						if (valign == VAlign::Center) {
							REAL offsety =
								(box.Height -
								 box1.Height) /
								2;
							y1 += offsety;
							y2 += offsety;
						} else if (valign ==
							   VAlign::Bottom) {
							REAL offsety =
								box.Height -
								box1.Height +
								font_width / 2;
							y1 += offsety;
							y2 += offsety;
						}
					}
					stat = graphics_bitmap.DrawLine(
						&pen, x1, y1, x2, y2);
				}
				warn_stat("graphics_bitmap.DrawString");
			}
		}
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

const char *SlideTextSource::GetMainSlideString(const char *str)
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

void SlideTextSource::SlideTextCustomCommand(obs_data_t *cmd)
{
	const char *cmdType = obs_data_get_string(cmd, "type");
	if (strcmp("replay", cmdType) == 0) {
		int index = texts.size() - 1;
		curIndex = index >= 0 ? index : 0;
	}
}

void SlideTextSource::SlideUpdate(obs_data_t *s)
{
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
	const char *new_front_color = obs_data_get_string(s, S_FONTCOLOR);
	bool new_vertical = obs_data_get_bool(s, S_DISPLAY) == false ? true
								     : false;
	texts.clear();
	// 这里兼容一下老版本
	if (strcmp(new_color, "#00000000") == 0)
		fill = false;
	else
		fill = true;

	if (strcmp(new_color, "#FFF244F") == 0) {
		new_color = "#00000000";
	}

	if (strcmp(new_front_color, "#FFF244F") == 0) {
		new_front_color = "#00000000";
	}
	obs_data_array_t *array = obs_data_get_array(s, "textStrings");
	int count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		string value = obs_data_get_string(item, "value");
		bool hide = obs_data_get_bool(item, "hidden");

		if (!hide)
			texts.push_back(value);
		obs_data_release(item);
	}
	obs_data_array_release(array);

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
	UpdateSlideFont();
	/* ----------------------------- */
	string str_color = new_front_color;
	str_color = str_color.substr(1);
	str_color = "0x" + str_color;
	sscanf(str_color.c_str(), "%x", &color2);        //十六进制转数字
	sscanf(str_color.c_str(), "%x", &color);         //十六进制转数字
	sscanf(str_color.c_str(), "%x", &outline_color); //十六进制转数字
	str_color = new_color;
	str_color = str_color.substr(1);
	str_color = "0x" + str_color;
	sscanf(str_color.c_str(), "%x", &bk_color); //十六进制转数字
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

	update_time_elapsed = 0.0f;

	/* ----------------------------- */

	int size_count = texts.size();
	if (size_count > 0) {
		if (curIndex > (size_count - 1))
			curIndex = 0;

		text = to_wide(texts.at(curIndex).c_str());
	} else {
		text = L"";
		ver_texts.clear();
	}

	if (vertical) {
		GenerateVerSlideText(text, ver_texts);
	}
	CalculateMaxSize();
	RenderSlideText();
}

wstring SlideTextSource::GetNextString()
{
	wstring text_tmp;
	curIndex = (curIndex + 1) > texts.size() - 1 ? 0 : (curIndex + 1);
	update_time = 0.0f;
	if (vertical) {
		text_tmp = to_wide(texts.at(curIndex).c_str());
	} else {
		text_tmp = to_wide(texts.at(curIndex).c_str());
	}
	return text_tmp;
}

void SlideTextSource::SlideTick(float seconds)
{

	if (texts.size() > 0) {
		if (texts.size() == 1)
			return;

		if (update_time < animate_time) {
			update_time = update_time + seconds;
			RenderSlideText();
		} else {
			int index = 0;
			text = GetNextString();
			while (text.size() == 0) {
				index++;
				text = GetNextString();
				if (index > texts.size())
					break;
			}
			if (text.size() > 0 && vertical)
				GenerateVerSlideText(text, ver_texts);
		}
	}
}

void SlideTextSource::SlideRender()
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

void SlideTextSource::CaclculateSpeed()
{
	switch (speed) {
	case 1:
		animate_time = 3.0f;
		break;
	case 2:
		animate_time = 2.0f;
		break;
	case 3:
		animate_time = 1.0f;
		break;
	default:
		break;
	}
}

bool SlideTextSource::VerDeleteLineLarge()
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
