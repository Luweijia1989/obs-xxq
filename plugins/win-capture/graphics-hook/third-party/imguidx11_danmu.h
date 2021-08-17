#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <Windows.h>
#include <d3d11.h>

void imgui_init_dx11(ID3D11Device *device, HWND hwnd, ID3D11DeviceContext *context);
void imgui_paint_dx11();
void imgui_finish_dx11();
