#pragma once

#include <imgui/imgui.h>
#include <imgui_impl_dx9/imgui_impl_dx9.h>
#include <imgui_impl_win32.h>
#include <Windows.h>

void imgui_init(IDirect3DDevice9 *device, HWND hwnd);
void imgui_paint(IDirect3DDevice9 *device);
void imgui_before_reset();
void imgui_after_reset(IDirect3DDevice9 *device);
void imgui_finish();
