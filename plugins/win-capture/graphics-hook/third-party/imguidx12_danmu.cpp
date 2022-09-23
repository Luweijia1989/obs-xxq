#include "imguidx12_danmu.h"
#include "imgui_internal.h"
#include "../graphics-hook.h"
#include <string>
#include "json/json.h"
#include "shared_helper.h"
#include "draw_danmu.h"
#include <dxgi1_4.h>

#if defined _WIN64
typedef uint64_t uintx_t;
#else
typedef uint32_t uintx_t;
#endif

namespace DirectX12Interface {
ID3D12Device *Device = nullptr;
ID3D12DescriptorHeap *DescriptorHeapBackBuffers;
ID3D12DescriptorHeap *DescriptorHeapImGuiRender;
ID3D12GraphicsCommandList *CommandList;
ID3D12CommandQueue *CommandQueue;

struct _FrameContext {
	ID3D12CommandAllocator *CommandAllocator;
	ID3D12Resource *Resource;
	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle;
};

uintx_t BuffersCounts = -1;
_FrameContext *FrameContext;
}

static bool is_initialised = false;

void imgui_init_dx12(ID3D12Device *device, HWND hwnd, IDXGISwapChain *swap)
{
	if (is_initialised)
		return;

	if (!DirectX12Interface::CommandQueue)
		return;

	IDXGISwapChain3 *swap3;
	auto hr = swap->QueryInterface(__uuidof(IDXGISwapChain3), (void **)&swap3);
	if (!SUCCEEDED(hr))
		return;

	DirectX12Interface::Device = device;

	bool success = false;
	do {
		DXGI_SWAP_CHAIN_DESC Desc;
		swap3->GetDesc(&Desc);

		DirectX12Interface::BuffersCounts = Desc.BufferCount;
		DirectX12Interface::FrameContext = new DirectX12Interface::_FrameContext[DirectX12Interface::BuffersCounts];

		D3D12_DESCRIPTOR_HEAP_DESC DescriptorImGuiRender = {};
		DescriptorImGuiRender.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		DescriptorImGuiRender.NumDescriptors = DirectX12Interface::BuffersCounts;
		DescriptorImGuiRender.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		if (DirectX12Interface::Device->CreateDescriptorHeap(&DescriptorImGuiRender, IID_PPV_ARGS(&DirectX12Interface::DescriptorHeapImGuiRender)) !=
		    S_OK)
			break;

		ID3D12CommandAllocator *Allocator;
		if (DirectX12Interface::Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Allocator)) != S_OK)
			break;

		for (size_t i = 0; i < DirectX12Interface::BuffersCounts; i++) {
			DirectX12Interface::FrameContext[i].CommandAllocator = Allocator;
		}

		if (DirectX12Interface::Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Allocator, NULL,
								  IID_PPV_ARGS(&DirectX12Interface::CommandList)) != S_OK ||
		    DirectX12Interface::CommandList->Close() != S_OK)
			break;

		D3D12_DESCRIPTOR_HEAP_DESC DescriptorBackBuffers;
		DescriptorBackBuffers.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		DescriptorBackBuffers.NumDescriptors = DirectX12Interface::BuffersCounts;
		DescriptorBackBuffers.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		DescriptorBackBuffers.NodeMask = 1;

		if (DirectX12Interface::Device->CreateDescriptorHeap(&DescriptorBackBuffers, IID_PPV_ARGS(&DirectX12Interface::DescriptorHeapBackBuffers)) !=
		    S_OK)
			break;

		const auto RTVDescriptorSize = DirectX12Interface::Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = DirectX12Interface::DescriptorHeapBackBuffers->GetCPUDescriptorHandleForHeapStart();

		for (size_t i = 0; i < DirectX12Interface::BuffersCounts; i++) {
			ID3D12Resource *pBackBuffer = nullptr;
			DirectX12Interface::FrameContext[i].DescriptorHandle = RTVHandle;
			swap3->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
			DirectX12Interface::Device->CreateRenderTargetView(pBackBuffer, nullptr, RTVHandle);
			DirectX12Interface::FrameContext[i].Resource = pBackBuffer;
			RTVHandle.ptr += RTVDescriptorSize;
		}

		success = true;
	} while (0);

	swap3->Release();

	if (!success)
		return;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	StyleColorsYuer(nullptr);

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(DirectX12Interface::Device, DirectX12Interface::BuffersCounts, DXGI_FORMAT_R8G8B8A8_UNORM,
			    DirectX12Interface::DescriptorHeapImGuiRender, DirectX12Interface::DescriptorHeapImGuiRender->GetCPUDescriptorHandleForHeapStart(),
			    DirectX12Interface::DescriptorHeapImGuiRender->GetGPUDescriptorHandleForHeapStart());
	ImGui_ImplDX12_CreateDeviceObjects();

	//add_fonts();

	qBase::connect(g_sharedSize, "YuerGameDanmu");

	is_initialised = true;
}

void imgui_set_command_queue(ID3D12CommandQueue *cq)
{
	if (!DirectX12Interface::CommandQueue)
		DirectX12Interface::CommandQueue = cq;
}

void imgui_paint_dx12(IDXGISwapChain *swap)
{
	if (capture_active() && is_initialised) {
		Json::Value root;
		/*if (!checkDanmu(root))
			return;*/

		IDXGISwapChain3 *swap3;
		auto hr = swap->QueryInterface(__uuidof(IDXGISwapChain3), (void **)&swap3);
		if (!SUCCEEDED(hr))
			return;

		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();

		render_danmu(root);

		DirectX12Interface::_FrameContext &CurrentFrameContext = DirectX12Interface::FrameContext[swap3->GetCurrentBackBufferIndex()];
		CurrentFrameContext.CommandAllocator->Reset();

		D3D12_RESOURCE_BARRIER Barrier;
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		Barrier.Transition.pResource = CurrentFrameContext.Resource;
		Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		DirectX12Interface::CommandList->Reset(CurrentFrameContext.CommandAllocator, nullptr);
		DirectX12Interface::CommandList->ResourceBarrier(1, &Barrier);
		DirectX12Interface::CommandList->OMSetRenderTargets(1, &CurrentFrameContext.DescriptorHandle, FALSE, nullptr);
		DirectX12Interface::CommandList->SetDescriptorHeaps(1, &DirectX12Interface::DescriptorHeapImGuiRender);

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), DirectX12Interface::CommandList);
		Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		DirectX12Interface::CommandList->ResourceBarrier(1, &Barrier);
		DirectX12Interface::CommandList->Close();
		DirectX12Interface::CommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList *const *>(&DirectX12Interface::CommandList));
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

	bool deleted = false;
	for (UINT i = 0; i < DirectX12Interface::BuffersCounts; i++) {
		if (DirectX12Interface::FrameContext[i].Resource) {
			DirectX12Interface::FrameContext[i].Resource->Release();
			DirectX12Interface::FrameContext[i].Resource = NULL;
		}

		if (!deleted) {
			if (DirectX12Interface::FrameContext[i].CommandAllocator) {
				DirectX12Interface::FrameContext[i].CommandAllocator->Release();
				DirectX12Interface::FrameContext[i].CommandAllocator = NULL;
			}
			deleted = true;
		}
	}

	if (DirectX12Interface::CommandList) {
		DirectX12Interface::CommandList->Release();
		DirectX12Interface::CommandList = NULL;
	}
	if (DirectX12Interface::DescriptorHeapBackBuffers) {
		DirectX12Interface::DescriptorHeapBackBuffers->Release();
		DirectX12Interface::DescriptorHeapBackBuffers = NULL;
	}
	if (DirectX12Interface::DescriptorHeapImGuiRender) {
		DirectX12Interface::DescriptorHeapImGuiRender->Release();
		DirectX12Interface::DescriptorHeapImGuiRender = NULL;
	}

#ifdef DX12_ENABLE_DEBUG_LAYER
	IDXGIDebug1 *pDebug = NULL;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug)))) {
		pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
		pDebug->Release();
	}
#endif
}
