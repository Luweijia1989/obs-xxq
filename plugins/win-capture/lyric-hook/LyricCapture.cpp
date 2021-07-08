#include "GraphicsCaptureHook.h"
#include <atlimage.h>

typedef BOOL (WINAPI *UpdateLayeredWindowProc)(
	HWND          hWnd,
	HDC           hdcDst,
	POINT         *pptDst,
	SIZE          *psize,
	HDC           hdcSrc,
	POINT         *pptSrc,
	COLORREF      crKey,
	BLENDFUNCTION *pblend,
	DWORD         dwFlags
);

UpdateLayeredWindowProc YppUpdateLayeredWindow = NULL;
HookData                hookUpdateLayeredWindow;
BYTE *bits = NULL;

BOOL WriteBmpM(HDC hdc, RECT rect)
{
	HDC hDCMem = ::CreateCompatibleDC(hdc);//创建兼容DC
	HBITMAP hBitMap = ::CreateCompatibleBitmap(hdc, rect.right, rect.bottom);//创建兼容位图
	HBITMAP hOldMap = (HBITMAP)::SelectObject(hDCMem, hBitMap);//将位图选入DC，并保存返回值

	::BitBlt(hDCMem, 0, 0, rect.right, rect.bottom, hdc, 0, 0, SRCCOPY);//将屏幕DC的图象复制到内存DC中

	CImage image;
	image.Attach(hBitMap);
	image.Save(TEXT("E:\\llllll.png"));
	image.Detach();

	::SelectObject(hDCMem, hOldMap);//选入上次的返回值
	//释放
	::DeleteObject(hBitMap);
	::DeleteDC(hDCMem);
	return TRUE;
}

static BOOL WINAPI UpdateLayeredWindowHook(HWND hWnd, HDC hdcDst, POINT *pptDst,
                                           SIZE *psize, HDC hdcSrc,
                                           POINT *pptSrc, COLORREF crKey,
                                           BLENDFUNCTION *pblend,
                                           DWORD dwFlags) {

	RUNEVERYRESET logOutput << CurrentTimeString() << "UpdateLayeredWindow Called" << endl;

	

#if OLDHOOKS
	hookUpdateLayeredWindow.Unhook();
	RECT rec;
	rec.left = 0;
	rec.top = 0;
	rec.right = psize->cx;
	rec.bottom = psize->cy;
	WriteBmpM(hdcSrc, rec);
	BOOL bResult = UpdateLayeredWindow(hWnd, hdcDst, pptDst, psize, hdcSrc, pptSrc, crKey, pblend, dwFlags);

	/*HDC hMemDC = CreateCompatibleDC(hdcSrc);

	BITMAPINFO bmpInfo = {};
	bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmpInfo.bmiHeader.biBitCount = 32;
	bmpInfo.bmiHeader.biWidth = psize->cx;
	bmpInfo.bmiHeader.biHeight = psize->cy;
	bmpInfo.bmiHeader.biPlanes = 1;

	BYTE *pDataSrc = 0;
	HBITMAP hBitmap = CreateDIBSection(hMemDC, &bmpInfo, DIB_RGB_COLORS, (void**)&pDataSrc, NULL, 0);
	HGDIOBJ hOldBMP = ::SelectObject(hMemDC, hBitmap);

	::BitBlt(hMemDC, 0, 0, psize->cx, psize->cy, hdcSrc, 0, 0, SRCCOPY);
	
	::SelectObject(hMemDC, hOldBMP);
	::DeleteDC(hMemDC);
	::DeleteObject(hBitmap);*/
	hookUpdateLayeredWindow.Rehook();
	
#else
	BOOL bResult = ((UpdateLayeredWindowProc)hookUpdateLayeredWindow.origFunc)(hWnd, hdcDst, pptDst, psize, hdcSrc, pptSrc, crKey, pblend, dwFlags);
#endif

	return bResult;
}

bool InitLyricCapture()
{
	bool bSuccess = false;

	HMODULE user32 = GetModuleHandle(TEXT("User32.dll"));
	if (user32)
	{
		YppUpdateLayeredWindow = (UpdateLayeredWindowProc)GetProcAddress(user32, "UpdateLayeredWindow");

		if (YppUpdateLayeredWindow)
		{
			hookUpdateLayeredWindow.Hook((FARPROC)YppUpdateLayeredWindow, (FARPROC)UpdateLayeredWindowHook);
			bSuccess = true;
		}

		if (bSuccess)
		{
			hookUpdateLayeredWindow.Rehook();
		}
	}

	return bSuccess;
}
