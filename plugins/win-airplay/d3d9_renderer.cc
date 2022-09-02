#include "d3d9_renderer.h"
#include <util/base.h>

#define DX_SAFE_RELEASE(p)              \
	{                               \
		if (p) {                \
			(p)->Release(); \
			(p) = NULL;     \
		}                       \
	}

D3D9Renderer::D3D9Renderer() {}

D3D9Renderer::~D3D9Renderer()
{
	Destroy();
}

bool D3D9Renderer::Init()
{
	hwnd_ = GetDesktopWindow();

	if (!CreateDevice()) {
		return false;
	}

	return true;
}

void D3D9Renderer::Destroy()
{
	DX_SAFE_RELEASE(render_surface_);
	DX_SAFE_RELEASE(output_texture_);
	DX_SAFE_RELEASE(d3d9_device_);
	DX_SAFE_RELEASE(d3d9_);
}

IDirect3DDevice9 *D3D9Renderer::GetDevice()
{
	return d3d9_device_;
}

bool D3D9Renderer::CreateDevice()
{
	D3DCAPS9 d3d9_caps_;
	D3DPRESENT_PARAMETERS present_params_;
	memset(&d3d9_caps_, 0, sizeof(D3DCAPS9));
	memset(&present_params_, 0, sizeof(D3DPRESENT_PARAMETERS));

	HRESULT hr = S_OK;

	hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9_);
	if (!d3d9_) {
		blog(LOG_DEBUG, "Direct3DCreate9() failed.");
		return false;
	}

	hr = d3d9_->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
				  &d3d9_caps_);
	if (FAILED(hr)) {
		blog(LOG_DEBUG, "IDirect3D9::GetDeviceCaps() failed, %x", hr);
		return false;
	}

	int behavior_flags = D3DCREATE_MULTITHREADED;
	if (d3d9_caps_.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) {
		behavior_flags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	} else {
		behavior_flags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}

	D3DDISPLAYMODE disp_mode;
	d3d9_->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &disp_mode);

	present_params_.BackBufferWidth = 640;
	present_params_.BackBufferHeight = 480;
	present_params_.BackBufferFormat = disp_mode.Format; //D3DFMT_X8R8G8B8;
	present_params_.BackBufferCount = 1;
	present_params_.MultiSampleType = D3DMULTISAMPLE_NONE;
	present_params_.MultiSampleQuality = 0;
	present_params_.SwapEffect = D3DSWAPEFFECT_COPY;
	present_params_.hDeviceWindow = hwnd_;
	present_params_.Windowed = TRUE;
	present_params_.Flags = D3DPRESENTFLAG_VIDEO;
	present_params_.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	present_params_.PresentationInterval =
		0; // D3DPRESENT_INTERVAL_IMMEDIATE;

	hr = d3d9_->CreateDeviceEx(D3DADAPTER_DEFAULT, // primary adapter
				   D3DDEVTYPE_HAL,     // device type
				   hwnd_, // window associated with device
				   behavior_flags,   // vertex processing
				   &present_params_, // present parameters
				   NULL,
				   &d3d9_device_ // return created device
	);

	return true;
}

bool D3D9Renderer::CreateTexture(int width, int height)
{
	if (!d3d9_device_) {
		return false;
	}

	DX_SAFE_RELEASE(output_texture_);
	DX_SAFE_RELEASE(render_surface_);

	HRESULT hr = S_OK;

	hr = d3d9_device_->CreateTexture(width, height, 1,
					 D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8,
					 D3DPOOL_DEFAULT, &output_texture_,
					 &shared_handler_);

	if (FAILED(hr)) {
		blog(LOG_DEBUG, "IDirect3DDevice9::CreateTexture() failed, %x",
		     hr);
		return false;
	}

	hr = output_texture_->GetSurfaceLevel(0, &render_surface_);
	if (FAILED(hr)) {
		blog(LOG_DEBUG,
		     "IDirect3DTexture9::GetSurfaceLevel() failed, %x", hr);
		DX_SAFE_RELEASE(output_texture_);

		return false;
	}

	width_ = width;
	height_ = height;
	return true;
}

void D3D9Renderer::RenderFrame(AVFrame *frame)
{
	if (!d3d9_device_) {
		return;
	}

	LPDIRECT3DSURFACE9 surface = (LPDIRECT3DSURFACE9)frame->data[3];

	D3DSURFACE_DESC desc;
	surface->GetDesc(&desc);

	if (width_ != frame->width || height_ != frame->height) {
		if (!CreateTexture(frame->width, frame->height)) {
			return;
		}
	}

	RECT src_rect = {0, 0, frame->width, frame->height};
	d3d9_device_->StretchRect(surface, &src_rect, render_surface_, NULL,
				  D3DTEXF_LINEAR);
}
