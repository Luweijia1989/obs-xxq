#include "imguidx12_danmu.h"
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

void imgui_init_dx12(ID3D12Device *device, int num_frames_in_flight,
		DXGI_FORMAT rtv_format, ID3D12DescriptorHeap *cbv_srv_heap,
		D3D12_CPU_DESCRIPTOR_HANDLE font_srv_cpu_desc_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE font_srv_gpu_desc_handle, HWND hwnd)
{
	if (is_initialised)
		return;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();

	StyleColorsYuer(nullptr);

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device, num_frames_in_flight, DXGI_FORMAT_R8G8B8A8_UNORM, cbv_srv_heap, font_srv_cpu_desc_handle, font_srv_gpu_desc_handle);

	add_fonts();

	qBase::connect(g_sharedSize, "YuerGameDanmu");

	is_initialised = true;
}

void imgui_paint_dx12()
{
	if (capture_active() && is_initialised) {
		Json::Value root;
		if (!checkDanmu(root))
			return;

		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		// Start the Dear ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();

		render_danmu(root);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), nullptr);
	}
}

void imgui_finish_dx12()
{
	if (is_initialised) {
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		is_initialised = false;
	}
}
