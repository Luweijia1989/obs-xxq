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
	InstalledFontCollection fonts;
	INT found;
	int count = fonts.GetFamilyCount();
	FontFamily *fontFamily = new FontFamily[count];
	fonts.GetFamilies(count, fontFamily, &found);
	for (int i = 0; i < fonts.GetFamilyCount(); i++) {
		WCHAR wInstallFaceName[LF_FACESIZE];
		fontFamily[i].GetFamilyName(wInstallFaceName);
		if (wcscmp(fontName, wInstallFaceName) == 0) {
			bRtn = true;
			break;
		}
	}
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
