#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx10.h>
#include <imgui_impl_win32.h>
#include <Windows.h>

void imgui_init(ID3D10Device *device, HWND hwnd);
void imgui_paint();
void imgui_finish();
