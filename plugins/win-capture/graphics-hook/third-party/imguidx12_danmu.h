#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx9.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <Windows.h>

void imgui_init(IDirect3DDevice9 *device, HWND hwnd);
void imgui_init(ID3D11Device *device, HWND hwnd, ID3D11DeviceContext *context);
void imgui_paint();
void imgui_before_reset();
void imgui_after_reset(IDirect3DDevice9 *device);
void imgui_finish();
