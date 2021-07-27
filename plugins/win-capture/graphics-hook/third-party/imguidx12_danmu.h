#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <Windows.h>
#include <d3d12.h>

void imgui_init_dx12(ID3D12Device *device, int num_frames_in_flight,
		     DXGI_FORMAT rtv_format, ID3D12DescriptorHeap *cbv_srv_heap,
		     D3D12_CPU_DESCRIPTOR_HANDLE font_srv_cpu_desc_handle,
		     D3D12_GPU_DESCRIPTOR_HANDLE font_srv_gpu_desc_handle, HWND hwnd);
void imgui_paint_dx12();
void imgui_finish_dx12();
