#include "imguidx11_danmu.h"
#include "imgui_internal.h"
#include "../graphics-hook.h"
#include <d3d9.h>
#include <string>
#include "json/json.h"
#include "shared_helper.h"
#include "draw_danmu.h"

static bool is_initialised = false;
static ID3D11RenderTargetView *RenderTargetView = NULL;
static ID3D11DeviceContext *Context;

void imgui_init_dx11(IDXGISwapChain *swap, ID3D11Device *device, HWND hwnd, ID3D11DeviceContext *c)
{
	if (is_initialised)
		return;

	Context = c;

	ID3D11Texture2D *pBackBuffer = nullptr;
	swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&pBackBuffer);
	device->CreateRenderTargetView(pBackBuffer, NULL, &RenderTargetView);
	pBackBuffer->Release();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();

	StyleColorsYuer(nullptr);

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(device, c);

	add_fonts();

	qBase::connect(g_sharedSize, "YuerGameDanmu");

	is_initialised = true;
}

void imgui_paint_dx11()
{
	if (capture_active() && is_initialised) {
		Json::Value root;
		if (!checkDanmu(root))
			return;

		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();

		render_danmu(root);

		ID3D11RenderTargetView *render_targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {nullptr};
		ID3D11DepthStencilView *zstencil_view = nullptr;
		Context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_targets, &zstencil_view);
		Context->OMSetRenderTargets(1, &RenderTargetView, NULL);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		Context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_targets, zstencil_view);
	}
}

void imgui_finish_dx11()
{
	if (is_initialised) {
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		is_initialised = false;
	}

	if (RenderTargetView) {
		RenderTargetView->Release();
		RenderTargetView = nullptr;
	}
}
