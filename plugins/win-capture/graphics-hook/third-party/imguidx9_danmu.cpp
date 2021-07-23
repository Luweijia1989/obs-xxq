#include "imguidx9_danmu.h"
#include "imgui_internal.h"
#include "../graphics-hook.h"
#include <d3d9.h>
#include <string>
#include "json/json.h"
#include "shared_helper.h"

// Pull in the reference WndProc handler to handle window messages.
extern IMGUI_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM,
							LPARAM);
static bool is_initialised = false;

static ImGuiWindowFlags window_flags = 0;
static int g_px = 8;
#define g_sharedSize (1024 * 8)
#define DanmuWidgetWidth 384
#define DanmuWidgetHeight 144
#define WidgetSpacing 16

ImVec2 danmuWidgetPos(int postType)
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
			vec.y = (v1.y - DanmuWidgetHeight) / 2;
		}
	} break;
	case 5: {
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		if (viewport) {
			ImVec2 v1 = viewport->WorkSize;
			vec.x = v1.x - DanmuWidgetWidth - WidgetSpacing;
			vec.y = (v1.y - DanmuWidgetHeight) / 2;
		}
	} break;
	case 6: {
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		if (viewport) {
			ImVec2 v1 = viewport->WorkSize;
			vec.x = WidgetSpacing;
			vec.y = v1.y - DanmuWidgetHeight - WidgetSpacing;
		}
	} break;
	case 7: {
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		if (viewport) {
			ImVec2 v1 = viewport->WorkSize;
			vec.x = (v1.x - DanmuWidgetWidth) / 2;
			vec.y = v1.y - DanmuWidgetHeight - WidgetSpacing;
		}
	} break;
	case 8: {
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		if (viewport) {
			ImVec2 v1 = viewport->WorkSize;
			vec.x = v1.x - DanmuWidgetWidth - WidgetSpacing;
			vec.y = v1.y - DanmuWidgetHeight - WidgetSpacing;
		}
	} break;
	default:
		break;
	}
	return vec;
}

void StyleColorsYuer(ImGuiStyle *dst)
{
	ImGuiStyle *style = dst ? dst : &ImGui::GetStyle();
	ImVec4 *colors = style->Colors;

	style->WindowBorderSize = 0.0f;
	style->ChildBorderSize = 8.0f;
	style->ItemSpacing = ImVec2(8, 0);
	style->WindowPadding = ImVec2(8.0f, 0.0f);

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

void addDanmu(Json::Value item, bool end = false)
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
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 8.0f));
	ImGui::BeginChild(id.c_str(),
			  ImVec2(0, ImGui::GetTextLineHeight() * rows + 15),
			  false, window_flags);

	Json::Value fieldArray = item["field"];
	for (int i = 0; i < fieldArray.size(); ++i) {
		Json::Value field = fieldArray[i];
		std::string textStr = field["text"].asString();
		char *text = &textStr[0];
		float width = ImGui::CalcTextSize(text).x;
		if (width > remainder) {
			const char *text_end = text + strlen(text);
			const char *p_remainder = g.Font->CalcWordWrapPositionA(
				1, text, text_end, remainder);

			if (p_remainder ==
			    text) // Wrap_width is too small to fit anything. Force displaying 1 character to minimize the height discontinuity.
				p_remainder++;

			char temp[256] = {0};
			memcpy(temp, text, p_remainder - text);

			ImGui::AlignTextToFramePadding();
			ImGui::TextColored(
				ImGui::ColorConvertU32ToFloat4(ARGBstringColor2UINT(
					field["color"].asString())),
				temp);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 8.0f);
			ImGui::PushTextWrapPos();
			//ImGui::AlignTextToFramePadding();
			ImGui::TextColored(
				ImGui::ColorConvertU32ToFloat4(ARGBstringColor2UINT(
					field["color"].asString())),
				p_remainder);
			ImGui::PopTextWrapPos();
		} else {
			ImGui::AlignTextToFramePadding();
			ImGui::TextColored(
				ImGui::ColorConvertU32ToFloat4(ARGBstringColor2UINT(
					field["color"].asString())),
				text);
			ImGui::SameLine(0, 0);
			remainder -= width;
		}
	}

	ImGui::EndChild();
	ImGui::PopStyleVar(1);

	if (!end) {
		std::string lineId = "line" + id;
		ImGui::BeginChild(lineId.c_str(), ImVec2(0, 1.0f));
		ImVec2 p = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y),
						    ImVec2(p.x + 384 - 16, p.y),
						    IM_COL32(255, 255, 255, 38.25),
						    1.0f);
		ImGui::EndChild();
	}
}

FORCEINLINE void draw_interface()
{
	char buff[g_sharedSize] = {0};
	qBase::readShare(g_sharedSize, buff);
	Json::Reader reader;
	Json::Value root;
	if (!reader.parse(buff, root, false))
		return;

	if (!root["isOpen"].asBool())
		return;

	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Hello, world!", NULL, window_flags); // Create a window called "Hello, world!" and append into it.
	ImGui::SetWindowSize(ImVec2(DanmuWidgetWidth, DanmuWidgetHeight));
	ImGui::SetWindowPos(danmuWidgetPos(root["posType"].asInt()));


	int fontSize = root["fontSize"].asInt();
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
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

std::string WStringToString(const std::wstring &wstr)
{
	std::string str;
	int nLen = (int)wstr.length();
	str.resize(nLen, ' ');
	int nResult = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)wstr.c_str(),
					  nLen, (LPSTR)str.c_str(), nLen, NULL,
					  NULL);
	if (nResult == 0) {
		return "";
	}
	return str;
}

void imgui_init(IDirect3DDevice9 *device, HWND hwnd)
{
	if (is_initialised)
		return;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	StyleColorsYuer(nullptr);

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX9_Init(device);
	//ImGui_ImplDX9_CreateDeviceObjects();

	TCHAR szBuffer[MAX_PATH] = { 0 };
	HMODULE hMod = NULL;
	TCHAR* dllName = L"graphics-hook64.dll";
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
		io.Fonts->AddFontFromFileTTF(
			fontPath.c_str(),
			18.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
		io.Fonts->AddFontFromFileTTF(
			fontPath.c_str(),
			20.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
		io.Fonts->AddFontFromFileTTF(
			fontPath.c_str(),
			22.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
	}

	qBase::connect(g_sharedSize, "YuerGameDanmu");

	is_initialised = true;
}

void imgui_paint()
{
	if (capture_active() && is_initialised) {
		draw_interface();
	}
}

void imgui_before_reset()
{
	ImGui_ImplDX9_InvalidateDeviceObjects();
}

void imgui_after_reset(IDirect3DDevice9 *device)
{
	D3DPRESENT_PARAMETERS g_d3dpp = {};
	HRESULT hr = device->Reset(&g_d3dpp);
	if (hr == D3DERR_INVALIDCALL)
		IM_ASSERT(0);
	ImGui_ImplDX9_CreateDeviceObjects();
}

void imgui_finish()
{
	if (is_initialised) {
		ImGui_ImplDX9_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		is_initialised = false;
	}
}
