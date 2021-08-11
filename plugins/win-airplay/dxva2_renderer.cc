#include "dxva2_renderer.h"

DXVA2Renderer::DXVA2Renderer()
{

}

DXVA2Renderer::~DXVA2Renderer()
{

}

void DXVA2Renderer::RenderFrame(AVFrame* frame)
{
	if (!d3d9_device_) {
		return;
	}

	LPDIRECT3DSURFACE9 surface = (LPDIRECT3DSURFACE9)frame->data[3];

	D3DSURFACE_DESC desc;
	surface->GetDesc(&desc);

	if (pixel_format_ != xop::PIXEL_FORMAT_NV12 ||
		width_ != frame->width ||height_ != frame->height) {
		if (!CreateTexture(frame->width, frame->height, xop::PIXEL_FORMAT_NV12)) {
			return;
		}
	}

	Begin();

	output_texture_ = input_texture_[xop::PIXEL_PLANE_ARGB].get();
	RECT src_rect = {0, 0, frame->width, frame->height};
	d3d9_device_->StretchRect(surface, &src_rect, output_texture_->GetSurface(), NULL, D3DTEXF_LINEAR);
	
//	Process();
//	End();
}

HANDLE DXVA2Renderer::GetTextureSharedHandle()
{
	return input_texture_[xop::PIXEL_PLANE_ARGB].get()->GetSharedHandler();
}
