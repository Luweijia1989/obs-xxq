#pragma once
#include <graphics/math-defs.h>
#include <util/platform.h>
#include <util/util.hpp>
#include <obs-module.h>
#include <sys/stat.h>
#include <windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <string>
#include <memory>
#include <vector>
#include <string>

using namespace std;
using namespace Gdiplus;

#define warning(format, ...)                                           \
	blog(LOG_WARNING, "[%s] " format, obs_source_get_name(source), \
	     ##__VA_ARGS__)

#define warn_stat(call)                                                   \
	do {                                                              \
		if (stat != Ok)                                           \
			warning("%s: %s failed (%d)", __FUNCTION__, call, \
				(int)stat);                               \
	} while (false)

#ifndef clamp
#define clamp(val, min_val, max_val) \
	if (val < min_val)           \
		val = min_val;       \
	else if (val > max_val)      \
		val = max_val;
#endif

#define MIN_SIZE_CX 2
#define MIN_SIZE_CY 2
#define MAX_SIZE_CX 16384
#define MAX_SIZE_CY 16384

#define MAX_AREA (4096LL * 4096LL)

/* ------------------------------------------------------------------------- */

#define S_FONT "font"
#define S_USE_FILE "read_from_file"
#define S_FILE "file"
#define S_TEXT "text"
#define S_COLOR "color"
#define S_GRADIENT "gradient"
#define S_GRADIENT_COLOR "gradient_color"
#define S_GRADIENT_DIR "gradient_dir"
#define S_GRADIENT_OPACITY "gradient_opacity"
#define S_ALIGN "align"
#define S_VALIGN "valign"
#define S_OPACITY "opacity"
#define S_BKCOLOR "bk_color"
#define S_BKOPACITY "bk_opacity"
#define S_VERTICAL "vertical"
#define S_OUTLINE "outline"
#define S_OUTLINE_SIZE "outline_size"
#define S_OUTLINE_COLOR "outline_color"
#define S_OUTLINE_OPACITY "outline_opacity"
#define S_CHATLOG_MODE "chatlog"
#define S_CHATLOG_LINES "chatlog_lines"
#define S_EXTENTS "extents"
#define S_EXTENTS_WRAP "extents_wrap"
#define S_EXTENTS_CX "extents_cx"
#define S_EXTENTS_CY "extents_cy"
#define S_TRANSFORM "transform"

#define S_ALIGN_LEFT "left"
#define S_ALIGN_CENTER "center"
#define S_ALIGN_RIGHT "right"

#define S_VALIGN_TOP "top"
#define S_VALIGN_CENTER S_ALIGN_CENTER
#define S_VALIGN_BOTTOM "bottom"

#define S_TRANSFORM_NONE 0
#define S_TRANSFORM_UPPERCASE 1
#define S_TRANSFORM_LOWERCASE 2

#define T_(v) obs_module_text(v)
#define T_FONT T_("Font")
#define T_USE_FILE T_("ReadFromFile")
#define T_FILE T_("TextFile")
#define T_TEXT T_("Text")
#define T_COLOR T_("Color")
#define T_GRADIENT T_("Gradient")
#define T_GRADIENT_COLOR T_("Gradient.Color")
#define T_GRADIENT_DIR T_("Gradient.Direction")
#define T_GRADIENT_OPACITY T_("Gradient.Opacity")
#define T_ALIGN T_("Alignment")
#define T_VALIGN T_("VerticalAlignment")
#define T_OPACITY T_("Opacity")
#define T_BKCOLOR T_("BkColor")
#define T_BKOPACITY T_("BkOpacity")
#define T_VERTICAL T_("Vertical")
#define T_OUTLINE T_("Outline")
#define T_OUTLINE_SIZE T_("Outline.Size")
#define T_OUTLINE_COLOR T_("Outline.Color")
#define T_OUTLINE_OPACITY T_("Outline.Opacity")
#define T_CHATLOG_MODE T_("ChatlogMode")
#define T_CHATLOG_LINES T_("ChatlogMode.Lines")
#define T_EXTENTS T_("UseCustomExtents")
#define T_EXTENTS_WRAP T_("UseCustomExtents.Wrap")
#define T_EXTENTS_CX T_("Width")
#define T_EXTENTS_CY T_("Height")
#define T_TRANSFORM T_("Transform")

#define T_FILTER_TEXT_FILES T_("Filter.TextFiles")
#define T_FILTER_ALL_FILES T_("Filter.AllFiles")

#define T_ALIGN_LEFT T_("Alignment.Left")
#define T_ALIGN_CENTER T_("Alignment.Center")
#define T_ALIGN_RIGHT T_("Alignment.Right")

#define T_VALIGN_TOP T_("VerticalAlignment.Top")
#define T_VALIGN_CENTER T_ALIGN_CENTER
#define T_VALIGN_BOTTOM T_("VerticalAlignment.Bottom")

#define T_TRANSFORM_NONE T_("Transform.None")
#define T_TRANSFORM_UPPERCASE T_("Transform.Uppercase")
#define T_TRANSFORM_LOWERCASE T_("Transform.Lowercase")

//////////////////////文字轮播源新增属性///////////////////////////
#define S_DIRECTION "direction"
#define S_SPEED "speed"
#define S_TEXTSTRING "textStrings"
#define S_DISPLAY "hDisplay"
#define S_HORALIGN "horizontalAlignment"
#define S_VERALIGN "verticalAlignment"
#define S_BOLD "bold"
#define S_ITALIC "italic"
#define S_STRIKEOUT "strikeout"
#define S_UNDERLINE "underline"
#define S_FONTSIZE "fontSize"
#define S_FILLCOLOR "fillColor"
#define S_FONTCOLOR "frontColor"
#define obs_data_get_uint32 (uint32_t) obs_data_get_int

enum SlideDirection { BottomToTop = 1, TopToBottom, RightToLeft, LeftToRight };

enum SlideSpeed { SpeedSlow = 1, SpeedMiddle, SpeedFast };

static time_t get_modified_timestamp(const char *filename)
{
	struct stat stats;
	if (os_stat(filename, &stats) != 0)
		return -1;
	return stats.st_mtime;
}

////////////////////////////////////////////////////////////////////////
/* ------------------------------------------------------------------------- */

static inline DWORD get_alpha_val(uint32_t opacity)
{
	return ((opacity * 255 / 100) & 0xFF) << 24;
}

static inline DWORD calc_color(uint32_t color, uint32_t opacity)
{
	return color & 0xFFFFFF | get_alpha_val(opacity);
}

static inline wstring to_wide(const char *utf8)
{
	wstring text;

	size_t len = os_utf8_to_wcs(utf8, 0, nullptr, 0);
	text.resize(len);
	if (len)
		os_utf8_to_wcs(utf8, 0, &text[0], len + 1);

	return text;
}

static inline uint32_t rgb_to_bgr(uint32_t rgb)
{
	return ((rgb & 0xFF) << 16) | (rgb & 0xFF00) | ((rgb & 0xFF0000) >> 16);
}

static bool IsInstallFont(const wchar_t *fontName)
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

static wstring FontPath(const wchar_t *fontName)
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
	else if (wcscmp(fontName, L"DIN-BoldItalic") == 0)
		path = L"\\resource\\font\\xyz_bolditalic.ttf";
	else if (wcscmp(fontName, L"DIN Alternate") == 0)
		path = L"\\resource\\font\\DIN Alternate Bold.ttf";
	return path;
}

/* ------------------------------------------------------------------------- */

template<typename T, typename T2, BOOL WINAPI deleter(T2)> class GDIObj {
	T obj = nullptr;

	inline GDIObj &Replace(T obj_)
	{
		if (obj)
			deleter(obj);
		obj = obj_;
		return *this;
	}

public:
	inline GDIObj() {}
	inline GDIObj(T obj_) : obj(obj_) {}
	inline ~GDIObj() { deleter(obj); }

	inline T operator=(T obj_)
	{
		Replace(obj_);
		return obj;
	}

	inline operator T() const { return obj; }

	inline bool operator==(T obj_) const { return obj == obj_; }
	inline bool operator!=(T obj_) const { return obj != obj_; }
};

using HDCObj = GDIObj<HDC, HDC, DeleteDC>;
using HFONTObj = GDIObj<HFONT, HGDIOBJ, DeleteObject>;
using HBITMAPObj = GDIObj<HBITMAP, HGDIOBJ, DeleteObject>;

/* ------------------------------------------------------------------------- */

enum class Align { Left, Center, Right };

enum class VAlign { Top, Center, Bottom };
