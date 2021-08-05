#pragma once

#include "renderer.h"
#include "d3d11_render_texture.h"
#include <mutex>

namespace xop {

class D3D11Renderer : public Renderer
{
public:
	D3D11Renderer();
	virtual ~D3D11Renderer();

	virtual bool Init(UINT w, UINT h);
	virtual void Destroy();
	
	virtual void Render(PixelFrame* frame);

	ID3D11Device* GetDevice();

	// sharpness: 0.0 to 10.0
	virtual void SetSharpen(float unsharp);

protected:
	bool InitDevice(UINT w, UINT h);
	bool CreateRenderer();
	bool CreateTexture(int width, int height, PixelFormat format);
	void Copy(PixelFrame* frame);
	void Process();

	void UpdateARGB(PixelFrame* frame);
	void UpdateI444(PixelFrame* frame);
	void UpdateI420(PixelFrame* frame);
	void UpdateNV12(PixelFrame* frame);

	std::mutex mutex_;

	D3D_DRIVER_TYPE   driver_type_ = D3D_DRIVER_TYPE_UNKNOWN;
	D3D_FEATURE_LEVEL feature_level_ = D3D_FEATURE_LEVEL_11_0;

	ID3D11Device* d3d11_device_            = NULL;
	ID3D11DeviceContext* d3d11_context_    = NULL;

	PixelFormat pixel_format_ = PIXEL_FORMAT_UNKNOW;
	UINT width_ = 0;
	UINT height_ = 0;
	UINT target_width_ = 0;
	UINT target_height_ = 0;

	ID3D11SamplerState* point_sampler_  = NULL;
	ID3D11SamplerState* linear_sampler_ = NULL;

	D3D11RenderTexture* output_texture_ = NULL;
	std::unique_ptr<D3D11RenderTexture> input_texture_[PIXEL_PLANE_MAX];
	std::unique_ptr<D3D11RenderTexture> render_target_[PIXEL_SHADER_MAX];

	float unsharp_ = 0.0;
	ID3D11Buffer* sharpen_constants_ = NULL;
};

}
