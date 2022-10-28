#pragma once

#include <imgui/imgui.h>
#include <imgui_impl_win32.h>
#include <Windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <imgui/imgui_impl_dx12.h>

void imgui_init_dx12(ID3D12Device *device, HWND hwnd, IDXGISwapChain *swap);
void imgui_set_command_queue(ID3D12CommandQueue *cq);
void imgui_paint_dx12(IDXGISwapChain *swap);
void imgui_finish_dx12();

