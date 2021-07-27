#include "imguidx9_danmu.h"
#include "imgui_internal.h"
#include "../graphics-hook.h"
#include <d3d9.h>
#include <string>
#include "json/json.h"
#include "shared_helper.h"
#include "draw_danmu.h"

// Pull in the reference WndProc handler to handle window messages.
extern IMGUI_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM,
							LPARAM);

static bool is_initialised = false;

FORCEINLINE void draw_interface()
{
	char buff[g_sharedSize] = {0};
	qBase::readShare(g_sharedSize, buff);
	Json::Reader reader;
	Json::Value root;
	if (!reader.parse(buff, root, false))
		return;

	if (!root["isOpen"].asBool())
		return;

	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();

	render_danmu(root);

	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void imgui_init(IDirect3DDevice9 *device, HWND hwnd)
{
	if (is_initialised)
		return;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	StyleColorsYuer(nullptr);

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX9_Init(device);
	//ImGui_ImplDX9_CreateDeviceObjects();

	add_fonts();

	qBase::connect(g_sharedSize, "YuerGameDanmu");

	is_initialised = true;
}

void imgui_paint()
{
	if (capture_active() && is_initialised) {
		draw_interface();
	}
}

void imgui_before_reset()
{
	ImGui_ImplDX9_InvalidateDeviceObjects();
}

void imgui_after_reset(IDirect3DDevice9 *device)
{
	D3DPRESENT_PARAMETERS g_d3dpp = {};
	HRESULT hr = device->Reset(&g_d3dpp);
	if (hr == D3DERR_INVALIDCALL)
		IM_ASSERT(0);
	ImGui_ImplDX9_CreateDeviceObjects();
}

void imgui_finish()
{
	if (is_initialised) {
		ImGui_ImplDX9_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		is_initialised = false;
	}
}
