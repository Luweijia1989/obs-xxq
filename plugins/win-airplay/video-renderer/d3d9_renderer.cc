#include "d3d9_renderer.h"
#include "log.h"

#include "shader/d3d9/shader_d3d9_yuv_bt601.h"
#include "shader/d3d9/shader_d3d9_yuv_bt709.h"
#include "shader/d3d9/shader_d3d9_sharpness.h"

using namespace xop;

ATOM MainWindow::wnd_class_ = 0;
const wchar_t MainWindow::kClassName[] = L"VideoRender_MainWindow";

MainWindow::MainWindow() {}

MainWindow::~MainWindow()
{
	Destroy();
}

bool MainWindow::Init(int pos_x, int pos_y, int width, int height)
{
	if (!RegisterWindowClass()) {
		return false;
	}

	if (IsWindow()) {
		return true;
	}

	wnd_ = ::CreateWindowExW(
		WS_EX_OVERLAPPEDWINDOW, kClassName, L"video-renderer-dxva2",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, pos_x,
		pos_y, width, height, NULL, NULL, GetModuleHandle(NULL), this);

	return wnd_ != NULL;
}

void MainWindow::Destroy()
{
	if (IsWindow()) {
		::DestroyWindow(wnd_);
		wnd_ = NULL;
	}
}

bool MainWindow::IsWindow()
{
	return wnd_ && ::IsWindow(wnd_) != FALSE;
}

HWND MainWindow::GetHandle()
{
	return wnd_;
}

bool MainWindow::OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result)
{
	switch (msg) {
	case WM_SIZE:
		//resize
		return true;
	case WM_DESTROY:
		return true;
	}

	return false;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	MainWindow *windows = reinterpret_cast<MainWindow *>(
		::GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (!windows && WM_CREATE == msg) {
		CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT *>(lp);
		windows = reinterpret_cast<MainWindow *>(cs->lpCreateParams);
		windows->wnd_ = hwnd;
		::SetWindowLongPtr(hwnd, GWLP_USERDATA,
				   reinterpret_cast<LONG_PTR>(windows));
	}

	LRESULT result = 0;
	if (windows) {
		bool handled = windows->OnMessage(msg, wp, lp, &result);
		if (!handled) {
			result = ::DefWindowProc(hwnd, msg, wp, lp);
		}
	} else {
		result = ::DefWindowProc(hwnd, msg, wp, lp);
	}

	return result;
}

bool MainWindow::RegisterWindowClass()
{
	if (wnd_class_) {
		return true;
	}

	WNDCLASSEXW wcex = {sizeof(WNDCLASSEX)};
	wcex.style = CS_DBLCLKS;
	wcex.hInstance = GetModuleHandle(NULL);
	wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wcex.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	wcex.lpfnWndProc = &WndProc;
	wcex.lpszClassName = kClassName;
	wnd_class_ = ::RegisterClassExW(&wcex);
	return wnd_class_ != 0;
}

#define DX_SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } } 

typedef struct
{
	FLOAT x, y, z;
	D3DCOLOR color;
	FLOAT u, v;
} Vertex;

static const DWORD FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;

D3D9Renderer::D3D9Renderer()
{
	memset(&d3d9_caps_, 0, sizeof(D3DCAPS9));
	memset(&present_params_, 0, sizeof(D3DPRESENT_PARAMETERS));
}

D3D9Renderer::~D3D9Renderer()
{
	Destroy();

	if (window)
		delete window;
}

bool D3D9Renderer::Init()
{
	window = new MainWindow;
	window->Init(0, 0, 640, 480);
	hwnd_ = window->GetHandle();

	if (!CreateDevice()) {
		return false;
	}

	return true;
}

void D3D9Renderer::Destroy()
{
	for (int i = 0; i < PIXEL_PLANE_MAX; i++) {
		input_texture_[i].reset();
	}

	for (int i = 0; i < PIXEL_SHADER_MAX; i++) {
		render_target_[i].reset();
	}

	DX_SAFE_RELEASE(back_buffer_);
	DX_SAFE_RELEASE(d3d9_device_);
	DX_SAFE_RELEASE(d3d9_);

	output_texture_ = NULL;
	pixel_format_ = PIXEL_FORMAT_UNKNOW;
}

void D3D9Renderer::Render(PixelFrame* frame)
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (!d3d9_device_) {
		return;
	}

	if (pixel_format_ != frame->format || 
		width_ != frame->width || 
		height_ != frame->height) {
		if (!CreateTexture(frame->width, frame->height, frame->format)) {
			return;
		}
	}

	Begin();
	Copy(frame);
	Process();
	End();
}

IDirect3DDevice9* D3D9Renderer::GetDevice()
{
	return d3d9_device_;
}

void D3D9Renderer::SetSharpen(float unsharp)
{
	unsharp_ = unsharp;
}

bool D3D9Renderer::CreateDevice()
{
	HRESULT hr = S_OK;
	RECT client_rect;

	if (!GetClientRect(hwnd_, &client_rect)) {
		LOG("GetClientRect() failed.");
		return false;
	}

	hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9_);
	if (!d3d9_) {
		LOG("Direct3DCreate9() failed.");
		return false;
	}

	hr = d3d9_->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3d9_caps_);
	if (FAILED(hr)) {
		LOG("IDirect3D9::GetDeviceCaps() failed, %x", hr);
		return false;
	}

	int behavior_flags = D3DCREATE_MULTITHREADED;
	if (d3d9_caps_.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) {
		behavior_flags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	}
	else {
		behavior_flags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}

	D3DDISPLAYMODE disp_mode;
	d3d9_->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &disp_mode);

	present_params_.BackBufferWidth =  static_cast<UINT>(client_rect.right - client_rect.left);
	present_params_.BackBufferHeight = static_cast<UINT>(client_rect.bottom - client_rect.top);
	present_params_.BackBufferFormat = disp_mode.Format; //D3DFMT_X8R8G8B8;
	present_params_.BackBufferCount = 1;
	present_params_.MultiSampleType = D3DMULTISAMPLE_NONE;
	present_params_.MultiSampleQuality = 0;
	present_params_.SwapEffect = D3DSWAPEFFECT_COPY;
	present_params_.hDeviceWindow = hwnd_;
	present_params_.Windowed = TRUE;
	present_params_.Flags = D3DPRESENTFLAG_VIDEO;
	present_params_.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	present_params_.PresentationInterval = 0;// D3DPRESENT_INTERVAL_IMMEDIATE;

	hr = d3d9_->CreateDeviceEx(
		D3DADAPTER_DEFAULT,    // primary adapter
		D3DDEVTYPE_HAL,        // device type
		hwnd_,                 // window associated with device
		behavior_flags,        // vertex processing
		&present_params_,	   // present parameters
		NULL,
		&d3d9_device_		   // return created device
	);        

	return true;
}

bool D3D9Renderer::CreateRender()
{
	if (!d3d9_device_) {
		return false;
	}

	HRESULT hr = S_OK;

	hr = d3d9_device_->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer_);
	if (FAILED(hr)) {
		LOG("IDirect3DDevice9::GetRenderTarget() failed, %x", hr);
		return false;
	}

	D3DSURFACE_DESC desc;
	hr = back_buffer_->GetDesc(&desc);
	DX_SAFE_RELEASE(back_buffer_);
	if (FAILED(hr)) {
		LOG("IDirect3DSurface9::GetRenderTarget() failed, %x", hr);
		return false;
	}

	for (int i = 0; i < PIXEL_SHADER_MAX; i++) {
		render_target_[i].reset(new D3D9RenderTexture(d3d9_device_));
	}

	render_target_[PIXEL_SHADER_YUV_BT601]->InitTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT);
	render_target_[PIXEL_SHADER_YUV_BT709]->InitTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT);
	render_target_[PIXEL_SHADER_SHARPEN]->InitTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT);
	
	render_target_[PIXEL_SHADER_YUV_BT601]->InitVertexShader();
	render_target_[PIXEL_SHADER_YUV_BT709]->InitVertexShader();
	render_target_[PIXEL_SHADER_SHARPEN]->InitVertexShader();

	render_target_[PIXEL_SHADER_YUV_BT601]->InitPixelShader(NULL, shader_d3d9_yuv_bt601, sizeof(shader_d3d9_yuv_bt601));
	render_target_[PIXEL_SHADER_YUV_BT709]->InitPixelShader(NULL, shader_d3d9_yuv_bt709, sizeof(shader_d3d9_yuv_bt709));
	render_target_[PIXEL_SHADER_SHARPEN]->InitPixelShader(NULL, shader_d3d9_sharpness, sizeof(shader_d3d9_sharpness));
	
	return true;
}

bool D3D9Renderer::CreateTexture(int width, int height, PixelFormat format)
{
	if (!d3d9_device_) {
		return false;
	}

	for (int i = 0; i < PIXEL_PLANE_MAX; i++) {
		input_texture_[i].reset(new D3D9RenderTexture(d3d9_device_));
	}

	if (format == PIXEL_FORMAT_I420) {
		UINT half_width  = (width + 1) / 2;
		UINT half_height = (height + 1) / 2;
		input_texture_[PIXEL_PLANE_Y]->InitTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT);
		input_texture_[PIXEL_PLANE_U]->InitTexture(half_width, half_height, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT);
		input_texture_[PIXEL_PLANE_V]->InitTexture(half_width, half_height, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT);
	}
	else if (format == PIXEL_FORMAT_I444) {
		input_texture_[PIXEL_PLANE_Y]->InitTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT);
		input_texture_[PIXEL_PLANE_U]->InitTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT);
		input_texture_[PIXEL_PLANE_V]->InitTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT);
	}	
	else if (format == PIXEL_FORMAT_ARGB) {
		input_texture_[PIXEL_PLANE_ARGB]->InitTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT);
	}
	else if (format == PIXEL_FORMAT_NV12) {
		HRESULT hr = S_OK;
		hr = d3d9_->CheckDeviceFormat(D3DADAPTER_DEFAULT, 
			D3DDEVTYPE_HAL, 
			D3DFMT_X8R8G8B8,
			0,
			D3DRTYPE_SURFACE, 
			(D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2')
		);
		if (FAILED(hr)) {
			LOG("Unsupported conversion from nv12 to rgb.");
			return false;
		}

		input_texture_[PIXEL_PLANE_ARGB]->InitTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, true);
		input_texture_[PIXEL_PLANE_NV12]->InitSurface(width, height, (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'), D3DPOOL_DEFAULT);
	}

	width_ = width;
	height_ = height;
	pixel_format_ = format;

	return true;
}

void D3D9Renderer::Begin()
{
	if (!d3d9_device_) {
		return ;
	}

	// set maximum ambient light
	d3d9_device_->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(255, 255, 255));

	// Turn off culling
	d3d9_device_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	// Turn off the zbuffer
	d3d9_device_->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

	// Turn off lights
	d3d9_device_->SetRenderState(D3DRS_LIGHTING, FALSE);

	// Enable dithering
	d3d9_device_->SetRenderState(D3DRS_DITHERENABLE, TRUE);

	// disable stencil
	d3d9_device_->SetRenderState(D3DRS_STENCILENABLE, FALSE);

	// manage blending
	d3d9_device_->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	d3d9_device_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	d3d9_device_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	d3d9_device_->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
	d3d9_device_->SetRenderState(D3DRS_ALPHAREF, 0x00);
	d3d9_device_->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);

	d3d9_device_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	d3d9_device_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

	d3d9_device_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	d3d9_device_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	d3d9_device_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	d3d9_device_->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	d3d9_device_->BeginScene();
}

void D3D9Renderer::Copy(PixelFrame* frame)
{
	if (!d3d9_device_) {
		return ;
	}

	if (frame->format == PIXEL_FORMAT_I420) {
		if (input_texture_[PIXEL_PLANE_Y] &&
			input_texture_[PIXEL_PLANE_U] &&
			input_texture_[PIXEL_PLANE_V]) {
			UpdateI420(frame);
		}
	}
	else if (frame->format == PIXEL_FORMAT_I444) {
		if (input_texture_[PIXEL_PLANE_Y] && 
			input_texture_[PIXEL_PLANE_U] && 
			input_texture_[PIXEL_PLANE_V]) {
			UpdateI444(frame);
		}
	}
	else if (frame->format == PIXEL_FORMAT_ARGB) {
		if (input_texture_[PIXEL_PLANE_ARGB]) {
			UpdateARGB(frame);
		}		
	}
	else if (frame->format == PIXEL_FORMAT_NV12) {
		if (input_texture_[PIXEL_PLANE_NV12]) {
			UpdateNV12(frame);
		}
	}
}

void D3D9Renderer::Process()
{
	if (!output_texture_) {
		return;
	}

	if (unsharp_ > 0) {
		float width = static_cast<float>(width_);
		float height = static_cast<float>(height_);
		D3D9RenderTexture* render_target = render_target_[PIXEL_SHADER_SHARPEN].get();
		render_target->Begin();
		render_target->SetTexture(0, output_texture_->GetTexture());
		render_target->SetConstant(0, &width, 1);
		render_target->SetConstant(1, &height, 1);
		render_target->SetConstant(2, &unsharp_, 1);
		render_target->Draw();
		render_target->End();
		render_target->SetTexture(0, NULL);
		output_texture_ = render_target;
	}
}

void D3D9Renderer::End()
{
	d3d9_device_->EndScene();
	//RECT viewport;
	//GetClientRect(hwnd_, &viewport);

	//if (output_texture_) {
	//	HRESULT hr = d3d9_device_->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer_);
	//	if (FAILED(hr)) {
	//		LOG("IDirect3DDevice9::GetRenderTarget() failed, %x", hr);
	//		return ;
	//	}

	//	d3d9_device_->StretchRect(output_texture_->GetSurface(), NULL, back_buffer_, NULL, D3DTEXF_NONE);
	//	DX_SAFE_RELEASE(back_buffer_);
	//	output_texture_ = NULL;
	//}
	//
	//d3d9_device_->EndScene();
	//d3d9_device_->Present(NULL, NULL, NULL, NULL);
}

void D3D9Renderer::UpdateARGB(PixelFrame* frame)
{
	HRESULT hr = S_OK;
	D3DLOCKED_RECT rect;

	IDirect3DTexture9* texture = input_texture_[PIXEL_PLANE_ARGB]->GetTexture();
	if (texture) {
		HRESULT hr = texture->LockRect(0, &rect, 0, 0);
		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[0];
			int dst_pitch = rect.Pitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[0];
			uint8_t* dst_data = (uint8_t*)rect.pBits;
			for (int i = 0; i < frame->height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
		}
		texture->UnlockRect(0);
		output_texture_ = input_texture_[PIXEL_PLANE_ARGB].get();
	}
}

void D3D9Renderer::UpdateI444(PixelFrame* frame)
{
	IDirect3DTexture9* y_texture = input_texture_[PIXEL_PLANE_Y]->GetTexture();
	IDirect3DTexture9* u_texture = input_texture_[PIXEL_PLANE_U]->GetTexture();
	IDirect3DTexture9* v_texture = input_texture_[PIXEL_PLANE_V]->GetTexture();

	D3DLOCKED_RECT rect;
	HRESULT hr = S_OK;

	if (y_texture) {
		hr = y_texture->LockRect(0, &rect, 0, 0);
		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[0];
			int dst_pitch = rect.Pitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[0];
			uint8_t* dst_data = (uint8_t*)rect.pBits;
			for (int i = 0; i < frame->height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
			y_texture->UnlockRect(0);
		}
	}

	if (u_texture) {
		hr = u_texture->LockRect(0, &rect, 0, 0);
		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[1];
			int dst_pitch = rect.Pitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[1];
			uint8_t* dst_data = (uint8_t*)rect.pBits;
			for (int i = 0; i < frame->height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
			u_texture->UnlockRect(0);
		}
	}

	if (v_texture) {
		hr = v_texture->LockRect(0, &rect, 0, 0);
		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[2];
			int dst_pitch = rect.Pitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[2];
			uint8_t* dst_data = (uint8_t*)rect.pBits;
			for (int i = 0; i < frame->height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
			v_texture->UnlockRect(0);
		}
	}

	D3D9RenderTexture* render_target = render_target_[PIXEL_SHADER_YUV_BT601].get();
	if (render_target) {
		render_target->Begin();		
		render_target->SetTexture(0, y_texture);
		render_target->SetTexture(1, u_texture);
		render_target->SetTexture(2, v_texture);
		render_target->Draw();
		render_target->End();
		render_target->SetTexture(0, NULL);
		render_target->SetTexture(1, NULL);
		render_target->SetTexture(2, NULL);
		output_texture_ = render_target;
	}
}

void D3D9Renderer::UpdateI420(PixelFrame* frame)
{
	IDirect3DTexture9* y_texture = input_texture_[PIXEL_PLANE_Y]->GetTexture();
	IDirect3DTexture9* u_texture = input_texture_[PIXEL_PLANE_U]->GetTexture();
	IDirect3DTexture9* v_texture = input_texture_[PIXEL_PLANE_V]->GetTexture();

	D3DLOCKED_RECT rect;
	HRESULT hr = S_OK;
	int half_width = (frame->width + 1) / 2;
	int half_height = (frame->height + 1) / 2;

	if (y_texture) {
		hr = y_texture->LockRect(0, &rect, 0, 0);
		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[0];
			int dst_pitch = rect.Pitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[0];
			uint8_t* dst_data = (uint8_t*)rect.pBits;
			for (int i = 0; i < frame->height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
			y_texture->UnlockRect(0);
		}
	}

	if (u_texture) {
		hr = u_texture->LockRect(0, &rect, 0, 0);
		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[1];
			int dst_pitch = rect.Pitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[1];
			uint8_t* dst_data = (uint8_t*)rect.pBits;
			for (int i = 0; i < half_height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
			u_texture->UnlockRect(0);
		}
	}

	if (v_texture) {
		hr = v_texture->LockRect(0, &rect, 0, 0);
		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[2];
			int dst_pitch = rect.Pitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[2];
			uint8_t* dst_data = (uint8_t*)rect.pBits;
			for (int i = 0; i < half_height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
			v_texture->UnlockRect(0);
		}
	}

	D3D9RenderTexture* render_target = render_target_[PIXEL_SHADER_YUV_BT601].get();
	if (render_target) {
		render_target->Begin();
		render_target->SetTexture(0, y_texture);
		render_target->SetTexture(1, u_texture);
		render_target->SetTexture(2, v_texture);
		render_target->Draw();
		render_target->End();
		render_target->SetTexture(0, NULL);
		render_target->SetTexture(1, NULL);
		render_target->SetTexture(2, NULL);
		output_texture_ = render_target;
	}
}

void D3D9Renderer::UpdateNV12(PixelFrame* frame)
{
	IDirect3DSurface9* surface = input_texture_[PIXEL_PLANE_NV12]->GetSurface();
	D3DLOCKED_RECT rect;

	if (surface) {
		HRESULT hr = surface->LockRect(&rect, NULL, 0);
		if (SUCCEEDED(hr)) {
			uint8_t* src_data = (uint8_t*)frame->plane[0];
			uint8_t* dst_data = (uint8_t*)rect.pBits;
			int src_pitch = frame->pitch[0];
			int dst_pitch = rect.Pitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;

			for (int i = 0; i < frame->height; i++) {
				memcpy(dst_data, src_data, pitch);
				src_data += src_pitch;
				dst_data += dst_pitch;
			}
			
			src_data = (uint8_t*)frame->plane[1];
			src_pitch = frame->pitch[1];
			pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			int half_height = frame->height / 2;

			for (int i = 0; i < half_height; i++) {
				memcpy(dst_data, src_data, pitch);
				src_data += src_pitch;
				dst_data += dst_pitch;
			}
			surface->UnlockRect();
		}	

		// NV12 To ARGB
		output_texture_ = input_texture_[PIXEL_PLANE_ARGB].get();
		d3d9_device_->StretchRect(surface, NULL, output_texture_->GetSurface(), NULL, D3DTEXF_LINEAR);
	}
}