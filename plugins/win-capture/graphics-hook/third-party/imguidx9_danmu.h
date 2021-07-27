#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx9.h>
#include <imgui_impl_win32.h>
#include <Windows.h>

void imgui_init_dx9(IDirect3DDevice9 *device, HWND hwnd);
void imgui_paint_dx9();
void imgui_before_reset_dx9();
void imgui_after_reset_dx9(IDirect3DDevice9 *device);
void imgui_finish_dx9();
