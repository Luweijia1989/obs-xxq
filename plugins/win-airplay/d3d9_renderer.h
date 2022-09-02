#pragma once

#include <d3d9.h>
extern "C" {
#include "libavformat/avformat.h"
}

class D3D9Renderer {
public:
	D3D9Renderer();
	~D3D9Renderer();

	bool Init();
	void Destroy();

	IDirect3DDevice9 *GetDevice();
	void RenderFrame(AVFrame *frame);
	HANDLE GetTextureSharedHandle() { return shared_handler_; }

protected:
	bool CreateDevice();
	bool CreateTexture(int width, int height);

	HWND hwnd_ = NULL;

	IDirect3D9Ex *d3d9_ = NULL;
	IDirect3DDevice9Ex *d3d9_device_ = NULL;

	int width_ = 0;
	int height_ = 0;
	IDirect3DTexture9 *output_texture_ = NULL;
	IDirect3DSurface9 *render_surface_ = NULL;
	HANDLE shared_handler_ = NULL;
};
