#include "imguidx11_danmu.h"
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

void imgui_init_dx11(ID3D11Device *device, HWND hwnd, ID3D11DeviceContext *context)
{
	if (is_initialised)
		return;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();

	StyleColorsYuer(nullptr);

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(device, context);

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

		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
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
}
