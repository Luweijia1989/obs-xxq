#include <windows.h>
#include "../obfuscate.h"
#include <dxgi.h>
#include <tchar.h>
#include <shlwapi.h>
#include "graphics-hook.h"

#include <detours.h>

typedef BOOL(WINAPI *UpdateLayeredWindowProc)(
	HWND hWnd, HDC hdcDst, POINT *pptDst, SIZE *psize, HDC hdcSrc,
	POINT *pptSrc, COLORREF crKey, BLENDFUNCTION *pblend, DWORD dwFlags);

typedef BOOL(WINAPI *UpdateLayeredWindowIndirectProc)(
	HWND hwnd, const UPDATELAYEREDWINDOWINFO *pULWInfo);

UpdateLayeredWindowProc RealUpdatelayeredwindow;
UpdateLayeredWindowIndirectProc RealUpdateLayeredWindowIndirect;

struct lyric_data {
	uint32_t cx;
	uint32_t cy;

	HWND window;
	bool compatibility;
	HDC hdc;
	HBITMAP bmp, old_bmp;
	BYTE *bits;

	struct shmem_data *shmem_info;
};

static struct lyric_data data = {};

static bool lyric_shmem_init(HWND window)
{
	if (!capture_init_shmem(&data.shmem_info, window, data.cx, data.cy,
				data.cx * 4, DXGI_FORMAT_B8G8R8A8_UNORM, true)) {
		return false;
	}

	hlog("lyric memory capture successful");
	return true;
}

static void lyric_data_free()
{
	capture_free();

	if (data.hdc) {
		SelectObject(data.hdc, data.old_bmp);
		DeleteDC(data.hdc);
		DeleteObject(data.bmp);
	}

	memset(&data, 0, sizeof(struct lyric_data));
}

static void lyric_data_init(uint32_t width, uint32_t height, HWND window)
{
	data.cx = width;
	data.cy = height;
	data.window = window;

	BITMAPINFO bi = {0};
	BITMAPINFOHEADER *bih = &bi.bmiHeader;
	bih->biSize = sizeof(BITMAPINFOHEADER);
	bih->biBitCount = 32;
	bih->biWidth = width;
	bih->biHeight = height;
	bih->biPlanes = 1;

	data.hdc = CreateCompatibleDC(NULL);
	data.bmp = CreateDIBSection(data.hdc, &bi, DIB_RGB_COLORS,
				    (void **)&data.bits, NULL, 0);
	data.old_bmp = (HBITMAP)SelectObject(data.hdc, data.bmp);

	bool success = lyric_shmem_init(window);
	if (!success)
		lyric_data_free();
}

static void lyric_capture(HDC hdc, uint32_t width, uint32_t height)
{
	if (capture_should_stop()) {
		lyric_data_free();
	}
	if (capture_should_init()) {
		lyric_data_init(width, height, WindowFromDC(hdc));
	}
	if (capture_ready()) {
		if (width != data.cx || height != data.cy) {
			if (width != 0 && height != 0) {
				lyric_data_free();
			}
			return;
		}

		BitBlt(data.hdc, 0, 0, width, height, hdc, 0, 0, SRCCOPY);
		shmem_copy_data(0, data.bits);
	}
}

size_t wchar_to_utf8(const wchar_t *in, size_t insize, char *out,
		     size_t outsize)
{
	int i_insize = (int)insize;
	int ret;

	if (i_insize == 0)
		i_insize = (int)wcslen(in);

	ret = WideCharToMultiByte(CP_UTF8, 0, in, i_insize, out, (int)outsize,
				  NULL, NULL);

	return (ret > 0) ? (size_t)ret : 0;
}

static BOOL WINAPI hook_update_layered_window(
	HWND hWnd, HDC hdcDst, POINT *pptDst, SIZE *psize, HDC hdcSrc,
	POINT *pptSrc, COLORREF crKey, BLENDFUNCTION *pblend, DWORD dwFlags)
{
	TCHAR buf[64] = {};
	GetWindowText(hWnd, buf, 512);

	char buf1[512] = {0};
	wchar_to_utf8(buf, 0, buf1, 512);

	bool isKw = false;
	char dll_path[MAX_PATH];
	DWORD size = GetModuleFileNameA(NULL, dll_path, MAX_PATH);
	if (size && strstr(dll_path, "kwmusic.exe")) {
		isKw = true;
	}

	if ((!isKw && strstr(buf1, "桌面歌词") != NULL) || (isKw && strstr(buf1, "") != NULL))
		lyric_capture(hdcSrc, psize->cx, psize->cy);

	return RealUpdatelayeredwindow(hWnd, hdcDst, pptDst, psize, hdcSrc, pptSrc, crKey,
			pblend, dwFlags);
}

static BOOL WINAPI hook_update_layered_window_indirect(
	HWND hwnd, const UPDATELAYEREDWINDOWINFO *pULWInfo)
{
	TCHAR buf[64] = {};
	GetWindowText(hwnd, buf, 512);

	char buf1[512] = {0};
	wchar_to_utf8(buf, 0, buf1, 512);

	if (strstr(buf1, "桌面歌词") != NULL)
		lyric_capture(pULWInfo->hdcSrc, pULWInfo->psize->cx,
			      pULWInfo->psize->cy);

	return RealUpdateLayeredWindowIndirect(hwnd, pULWInfo);
}

bool hook_lyric()
{
	HMODULE user_module = get_system_module("User32.dll");
	void *update_layered_window_addr = nullptr;
	void *update_layered_window_indirect_addr = nullptr;

	if (!user_module) {
		return false;
	}

	DetourTransactionBegin();

	update_layered_window_addr =
		GetProcAddress(user_module, "UpdateLayeredWindow");
	if (!update_layered_window_addr) {
		hlog("Invalid Lyric values");
		return true;
	}
	if (update_layered_window_addr) {
		RealUpdatelayeredwindow = (UpdateLayeredWindowProc)update_layered_window_addr;
		DetourAttach((PVOID *)&RealUpdatelayeredwindow, hook_update_layered_window);
	}

	update_layered_window_indirect_addr =
		GetProcAddress(user_module, "UpdateLayeredWindowIndirect");
	if (!update_layered_window_indirect_addr) {
		hlog("Invalid Lyric values");
		return true;
	}
	if (update_layered_window_indirect_addr) {
		RealUpdateLayeredWindowIndirect = (UpdateLayeredWindowIndirectProc)update_layered_window_indirect_addr;
		DetourAttach((PVOID *)&RealUpdateLayeredWindowIndirect, hook_update_layered_window_indirect);
	}

	const LONG error = DetourTransactionCommit();
	const bool success = error == NO_ERROR;
	if (success) {
		hlog("Hooked RealUpdatelayeredwindow");
		hlog("Hooked RealUpdateLayeredWindowIndirect");
		hlog("Hooked Lyric");
	} else {
		RealUpdatelayeredwindow = nullptr;
		RealUpdateLayeredWindowIndirect = nullptr;
		hlog("Failed to attach Detours hook: %ld", error);
	}
	return success;
}
