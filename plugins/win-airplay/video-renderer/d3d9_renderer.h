#pragma once

#include "renderer.h"
#include "d3d9_render_texture.h"
#include <windows.h>
#include <d3d11.h>

#include <mutex>

namespace xop {

class MainWindow {
public:
	MainWindow();
	virtual ~MainWindow();

	bool Init(int pos_x, int pos_y, int width, int height);
	void Destroy();

	bool IsWindow();
	HWND GetHandle();

	virtual bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result);

private:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp,
					LPARAM lp);
	static bool RegisterWindowClass();

	HWND wnd_ = NULL;

	static ATOM wnd_class_;
	static const wchar_t kClassName[];
};


class D3D9Renderer : public Renderer
{
public:
	D3D9Renderer();
	virtual ~D3D9Renderer();

	virtual bool Init();
	virtual void Destroy();

	virtual void Render(PixelFrame* frame);

	virtual IDirect3DDevice9* GetDevice();

	// sharpness: 0.0 to 10.0
	virtual void SetSharpen(float unsharp);

protected:
	bool CreateDevice();
	bool CreateRender();
	bool CreateTexture(int width, int height, PixelFormat format);

	void Begin();
	void Copy(PixelFrame* frame);
	void Process();
	void End();

	void UpdateARGB(PixelFrame* frame);
	void UpdateI444(PixelFrame* frame);
	void UpdateI420(PixelFrame* frame);
	void UpdateNV12(PixelFrame* frame);

	std::mutex mutex_;

	HWND hwnd_ = NULL;

	D3DCAPS9 d3d9_caps_;
	D3DPRESENT_PARAMETERS present_params_;

	IDirect3D9Ex*        d3d9_           = NULL;
	IDirect3DDevice9Ex*  d3d9_device_    = NULL;
	IDirect3DSurface9* back_buffer_    = NULL;

	HANDLE d3d11_shared_handler = NULL;
	
	PixelFormat pixel_format_ = PIXEL_FORMAT_UNKNOW;
	int width_  = 0;
	int height_ = 0;

	D3D9RenderTexture* output_texture_ = NULL;
	std::unique_ptr<D3D9RenderTexture> input_texture_[PIXEL_PLANE_MAX];
	std::unique_ptr<D3D9RenderTexture> render_target_[PIXEL_SHADER_MAX];

	float unsharp_ = 0.0;

	MainWindow *window = nullptr;
};

}
