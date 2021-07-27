#pragma once

#include <imgui/imgui.h>
#include "json/json.h"

#define g_sharedSize (1024 * 8)
#define DanmuWidgetWidth 384
#define DanmuWidgetHeight 144
#define WidgetSpacing 16

extern ImGuiWindowFlags window_flags;

void StyleColorsYuer(ImGuiStyle *dst);

ImVec2 danmuWidgetPos(int postType);

unsigned int ARGBstringColor2UINT(std::string color);

void addDanmu(Json::Value item, bool end = false);

void render_danmu(Json::Value &root);

std::string WStringToString(const std::wstring &wstr);

void add_fonts();

bool checkDanmu(Json::Value &root);
