#include "draw_danmu.h"
#include "imgui_internal.h"
#include <string>
#include "shared_helper.h"
#include "../graphics-hook.h"

static ImGuiWindowFlags window_flags = 0;

ImVec2 danmuWidgetPos(int postType, int type)
{
	ImVec2 vec(WidgetSpacing, WidgetSpacing);
	switch (postType) {
	case 0:
		break;
	case 1: {
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		if (viewport) {
			ImVec2 v1 = viewport->WorkSize;
			vec.x = (v1.x - DanmuWidgetWidth) / 2;
			vec.y = WidgetSpacing;
		}
	} break;
	case 2: {
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		if (viewport) {
			ImVec2 v1 = viewport->WorkSize;
			vec.x = v1.x - DanmuWidgetWidth - WidgetSpacing;
			vec.y = WidgetSpacing;
		}
	} break;
	case 3: {
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		if (viewport) {
			ImVec2 v1 = viewport->WorkSize;
			vec.x = WidgetSpacing;
			vec.y = (v1.y - danmuWidgetHeight(type)) / 2;
		}
	} break;
	case 5: {
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		if (viewport) {
			ImVec2 v1 = viewport->WorkSize;
			vec.x = v1.x - DanmuWidgetWidth - WidgetSpacing;
			vec.y = (v1.y - danmuWidgetHeight(type)) / 2;
		}
	} break;
	case 6: {
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		if (viewport) {
			ImVec2 v1 = viewport->WorkSize;
			vec.x = WidgetSpacing;
			vec.y = v1.y - danmuWidgetHeight(type) - WidgetSpacing;
		}
	} break;
	case 7: {
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		if (viewport) {
			ImVec2 v1 = viewport->WorkSize;
			vec.x = (v1.x - DanmuWidgetWidth) / 2;
			vec.y = v1.y - danmuWidgetHeight(type) - WidgetSpacing;
		}
	} break;
	case 8: {
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		if (viewport) {
			ImVec2 v1 = viewport->WorkSize;
			vec.x = v1.x - DanmuWidgetWidth - WidgetSpacing;
			vec.y = v1.y - danmuWidgetHeight(type) - WidgetSpacing;
		}
	} break;
	default:
		break;
	}
	return vec;
}

int danmuWidgetHeight(int type) {
	switch (type) {
	case 18:
		return 144;
	case 20:
		return 152;
	case 22:
		return 164;
	}
	return 144;
}

void StyleColorsYuer(ImGuiStyle *dst)
{
	ImGuiStyle *style = dst ? dst : &ImGui::GetStyle();
	ImVec4 *colors = style->Colors;

	style->WindowBorderSize = 0.0f;
	style->ChildBorderSize = 8.0f;
	style->ItemSpacing = ImVec2(8, 0);
	style->WindowPadding = ImVec2(8.0f, 8.0f);
	style->FramePadding = ImVec2(0, 0);

	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
	colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.29f, 0.48f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] =
		ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] =
		ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_Tab] = ImLerp(colors[ImGuiCol_Header],
				      colors[ImGuiCol_TitleBgActive], 0.80f);
	colors[ImGuiCol_TabHovered] = colors[ImGuiCol_HeaderHovered];
	colors[ImGuiCol_TabActive] = ImLerp(colors[ImGuiCol_HeaderActive],
					    colors[ImGuiCol_TitleBgActive],
					    0.60f);
	colors[ImGuiCol_TabUnfocused] =
		ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
	colors[ImGuiCol_TabUnfocusedActive] = ImLerp(
		colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.40f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] =
		ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(
		0.31f, 0.31f, 0.35f, 1.00f); // Prefer using Alpha=1.0 here
	colors[ImGuiCol_TableBorderLight] = ImVec4(
		0.23f, 0.23f, 0.25f, 1.00f); // Prefer using Alpha=1.0 here
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] =
		ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	window_flags |= ImGuiWindowFlags_NoTitleBar;
	window_flags |= ImGuiWindowFlags_NoScrollbar;
	window_flags |= ImGuiWindowFlags_NoScrollWithMouse;
	//window_flags |= ImGuiWindowFlags_MenuBar;
	window_flags |= ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoResize;
	window_flags |= ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoNav;
	window_flags |= ImGuiWindowFlags_NoInputs;
	//window_flags |= ImGuiWindowFlags_NoBackground;
	//window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	window_flags |= ImGuiWindowFlags_UnsavedDocument;
}

unsigned int ARGBstringColor2UINT(std::string color)
{
	if (color.size() != 8)
		return IM_COL32(169, 235, 255, 255);

	int r, g, b, a;
	char rr[2] = {0}, gg[2] = {0}, bb[2] = {0}, aa[2] = {0};
	memcpy(aa, &color[0], 2);
	memcpy(rr, &color[2], 2);
	memcpy(gg, &color[4], 2);
	memcpy(bb, &color[6], 2);

	sscanf(rr, "%x", &r);
	sscanf(gg, "%x", &g);
	sscanf(bb, "%x", &b);
	sscanf(aa, "%x", &a);

	return IM_COL32(r, g, b, a);
}

void cutDanmu(const char *text, float &remainder, bool last, std::string color)
{
	ImGuiContext &g = *GImGui;
	float width = ImGui::CalcTextSize(text).x;
	if (width > remainder) {
		const char *text_end = text + strlen(text);
		const char *p_remainder = g.Font->CalcWordWrapPositionA(
			1, text, text_end, remainder);

		bool add = false;
		if (p_remainder ==
		    text) // Wrap_width is too small to fit anything. Force displaying 1 character to minimize the height discontinuity.
		{
			p_remainder++;
			add = true;
		}
		char temp[256] = {0};
		memcpy(temp, text, p_remainder - text);

		remainder -= ImGui::CalcTextSize(temp).x;
		if (remainder >= 0) {
			ImGui::AlignTextToFramePadding();
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
						   ARGBstringColor2UINT(color)),
					   temp);
		} else {
			if (p_remainder > text && add)
				p_remainder--;
			ImGuiWindow *window = ImGui::GetCurrentWindow();
			if (window->SkipItems)
				return;

			ImGuiContext &g = *GImGui;
			const ImGuiLayoutType backup_layout_type =
				window->DC.LayoutType;
			window->DC.LayoutType = ImGuiLayoutType_Vertical;
			ImGui::ItemSize(ImVec2(0, 0));
			window->DC.LayoutType = backup_layout_type;
		}

		if (!last) {
			remainder = ImGui::GetColumnWidth();
			//ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 16.0f);
			cutDanmu(p_remainder, remainder, false, color);
		} else {
			//ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);
			ImGui::PushTextWrapPos();
			//ImGui::AlignTextToFramePadding();
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
						   ARGBstringColor2UINT(color)),
					   p_remainder);
			ImGui::PopTextWrapPos();
		}
	} else {
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
					   ARGBstringColor2UINT(color)),
				   text);
		remainder -= width;
		ImGui::SameLine(0, 0);
	}
}

void addDanmu(Json::Value item, bool end)
{
	ImGuiContext &g = *GImGui;
	float textWidth =
		ImGui::CalcTextSize(item["intactText"].asString().c_str()).x;
	float lineWidth = ImGui::GetColumnWidth();
	float remainder = lineWidth;
	if (lineWidth <= 0)
		return;
	int rows = textWidth / lineWidth;
	if (fmod(textWidth, lineWidth) > 0)
		rows += 1;

	std::string id = item["id"].asString();

	Json::Value fieldArray = item["field"];
	for (int i = 0; i < fieldArray.size(); ++i) {
		Json::Value field = fieldArray[i];
		std::string textStr = field["text"].asString();
		char *text = &textStr[0];
		cutDanmu(text, remainder, i == fieldArray.size() - 1,
			 field["color"].asString());
	}

	if (!end) {
		ImGuiWindow *window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return;

		ImGuiContext &g = *GImGui;
		const ImGuiLayoutType backup_layout_type =
			window->DC.LayoutType;
		window->DC.LayoutType = ImGuiLayoutType_Vertical;
		ImGui::ItemSize(ImVec2(0, 0));
		window->DC.LayoutType = backup_layout_type;

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 9.0f);
		std::string lineId = "line" + id;
		ImGui::BeginChild(lineId.c_str(), ImVec2(0, 1.0f));
		ImVec2 p = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(p.x, p.y), ImVec2(p.x + 384 - 16, p.y),
			IM_COL32(255, 255, 255, 38.25), 1.0f);
		ImGui::EndChild();
		ImGui::ItemSize(ImVec2(0, 9));
	}
}

void render_danmu(Json::Value &root)
{
	ImGui::NewFrame();

	int fontSize = root["fontSize"].asInt();
	ImGui::Begin(
		"Hello, world!", NULL,
		window_flags); // Create a window called "Hello, world!" and append into it.
	ImGui::SetWindowSize(ImVec2(DanmuWidgetWidth, danmuWidgetHeight(fontSize)));
	ImGui::SetWindowPos(danmuWidgetPos(root["posType"].asInt(), fontSize));

	ImGuiIO &io = ImGui::GetIO();
	for (int n = 0; n < io.Fonts->Fonts.Size; n++) {
		ImFont *font = io.Fonts->Fonts[n];
		if (font->FontSize == fontSize) {
			io.FontDefault = font;
			break;
		}
	}

	Json::Value array = root["danmu"];
	for (int i = 0; i < array.size(); ++i) {
		addDanmu(array[i], i == array.size() - 1);
	}

	ImGui::SetScrollHereY(1.0f);
	ImGui::End();

	//Rendering
	ImGui::EndFrame();
	ImGui::Render();
}

std::string WStringToString(const std::wstring &wstr)
{
	std::string str;
	int nLen = (int)wstr.length();
	int nResult = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), nLen, NULL, 0, NULL, NULL);
	if (nResult == 0) {
		hlog("WStringToString failed");
		return "";
	}

	char newStr[512] = { 0 };
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)nLen, newStr, nResult + 1,
			    NULL, NULL);
	str.assign(newStr);

	return str;
}

void add_fonts()
{
	ImGuiIO &io = ImGui::GetIO();

	TCHAR szBuffer[MAX_PATH] = {0};
	HMODULE hMod = NULL;
	TCHAR *dllName = L"graphics-hook64.dll";
#ifdef _WIN64
	dllName = L"graphics-hook64.dll";
#else
	dllName = L"graphics-hook32.dll";
#endif
	hMod = GetModuleHandle(dllName);
	if (hMod != NULL) {
		GetModuleFileName(hMod, szBuffer,
				  sizeof(szBuffer) / sizeof(TCHAR) - 1);

		std::wstring ss;
		ss.assign(szBuffer);
		size_t pos = ss.find_last_of(L'\\');
		ss.resize(pos);
		ss += L"\\..\\..\\..\\..\\resource\\font\\Alibaba-PuHuiTi-Regular.ttf";
		std::string fontPath = WStringToString(ss);
		hlog("add_fonts, path: %s", fontPath.data());
		io.Fonts->AddFontFromFileTTF(
			fontPath.c_str(), 18.0f, NULL,
			io.Fonts->GetGlyphRangesChineseFull());
		io.Fonts->AddFontFromFileTTF(
			fontPath.c_str(), 20.0f, NULL,
			io.Fonts->GetGlyphRangesChineseFull());
		io.Fonts->AddFontFromFileTTF(
			fontPath.c_str(), 22.0f, NULL,
			io.Fonts->GetGlyphRangesChineseFull());
	}
	else {
		hlog("get graphics-hook's dll path failed.");
	}
}

bool checkDanmu(Json::Value &root)
{
	char buff[g_sharedSize] = {0};
	qBase::readShare(g_sharedSize, buff);
	Json::Reader reader;
	if (!reader.parse(buff, root, false))
		return false;

	if (!root["isOpen"].asBool())
		return false;
	return true;
}
