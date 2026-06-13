#include "Application.h"
#include "ApplicationConfiguration.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"
#include "../Depends/glad/include/glad/glad.h"
#include "../Depends/GLFW/include/GLFW/glfw3.h"
#include "File.h"
#include "UI.h"
#include "Utilities.h"
#include "IconsFontAwesome5.h"
#include "Colors.h"
#include "IO.h"       // --- 追加 ---
#include <filesystem> // --- 追加 ---
#include <fstream>    // --- 追加 ---

namespace MikuMikuWorld
{
	ImGuiManager::ImGuiManager() {}

	Result ImGuiManager::initialize(GLFWwindow* window)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		configFilename = Application::getAppDir() + IMGUI_CONFIG_FILENAME;

		// --- 修正ここから：パスをワイド文字列に変換してから処理する ---
		std::wstring wConfigFilename = IO::mbToWideStr(configFilename);

		if (!std::filesystem::exists(wConfigFilename)) {
			std::ofstream file(wConfigFilename);
			if (file.is_open()) {
				file << R"([Window][###notes_timeline]
Pos=251,55
Size=804,954
Collapsed=0
DockId=0x00000001,0

[Window][###chart_properties]
Pos=1057,580
Size=264,429
Collapsed=0
DockId=0x0000000C,0

[Window][###note_properties]
Pos=1323,580
Size=264,429
Collapsed=0
DockId=0x0000000D,0

[Window][###options]
Pos=1589,580
Size=331,429
Collapsed=0
DockId=0x00000004,0

[Window][###presets]
Pos=0,612
Size=249,397
Collapsed=0
DockId=0x0000000A,1

[Window][###layers]
Pos=0,55
Size=249,555
Collapsed=0
DockId=0x00000009,0

[Window][###waypoints]
Pos=0,612
Size=249,397
Collapsed=0
DockId=0x0000000A,0

[Window][InvisibleWindow]
Pos=0,35
Size=1920,974
Collapsed=0

[Window][Debug##Default]
ViewportPos=210,160
ViewportId=0x9F5F46A1
Size=400,400
Collapsed=0

[Window][##app_toolbar]
Pos=0,20
Size=1920,35
Collapsed=0

[Window][###score_preview]
Pos=1057,55
Size=863,523
Collapsed=0
DockId=0x00000005,0

[Window][###settings]
Pos=585,214
Size=750,600
Collapsed=0

[Window][###unsaved_changes]
Pos=735,414
Size=450,200
Collapsed=0

[Window][Add Folder]
ViewportPos=826,473
ViewportId=0x52ADEEC5
Size=268,100
Collapsed=0

[Window][Delete Chart]
ViewportPos=573,435
ViewportId=0xA0915BA2
Size=362,118
Collapsed=0

[Window][Delete File]
ViewportPos=769,414
ViewportId=0x94B2CB3D
Size=381,226
Collapsed=0

[Window][GalleryScreen]
Pos=0,0
Size=1920,1009
Collapsed=0

[Window][ファイルの削除]
ViewportPos=752,422
ViewportId=0x38D7DE83
Size=396,194
Collapsed=0

[Table][0xD68DAD8B,2]
RefScale=16
Column 0  Weight=1.0000
Column 1  Width=89

[Table][0x519E09DE,2]
RefScale=16
Column 0  Width=159
Column 1  Weight=1.0000

[Docking][Data]
DockSpace           ID=0xF442860A Window=0xD8117908 Pos=0,58 Size=1920,974 Split=X
  DockNode          ID=0x00000007 Parent=0xF442860A SizeRef=249,954 Split=Y Selected=0xCB59F716
    DockNode        ID=0x00000009 Parent=0x00000007 SizeRef=249,555 Selected=0xCB59F716
    DockNode        ID=0x0000000A Parent=0x00000007 SizeRef=249,397 Selected=0xEC13F1BA
  DockNode          ID=0x00000008 Parent=0xF442860A SizeRef=1669,954 Split=X
    DockNode        ID=0x00000001 Parent=0x00000008 SizeRef=804,765 CentralNode=1 Selected=0x7832EA6E
    DockNode        ID=0x00000002 Parent=0x00000008 SizeRef=863,765 Split=Y
      DockNode      ID=0x00000005 Parent=0x00000002 SizeRef=250,523 Selected=0x3A83CA69
      DockNode      ID=0x00000006 Parent=0x00000002 SizeRef=250,429 Split=X Selected=0x25F8FF00
        DockNode    ID=0x00000003 Parent=0x00000006 SizeRef=530,829 Split=X Selected=0x25F8FF00
          DockNode  ID=0x0000000C Parent=0x00000003 SizeRef=264,429 Selected=0xD503FE8A
          DockNode  ID=0x0000000D Parent=0x00000003 SizeRef=264,429 Selected=0x25F8FF00
        DockNode    ID=0x00000004 Parent=0x00000006 SizeRef=331,829 Selected=0x0FDBD4A5
)";
				file.close();
			}
		}
		// --- 修正ここまで ---

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable |
		                  ImGuiConfigFlags_DpiEnableScaleViewports;

		io.ConfigWindowsMoveFromTitleBarOnly = true;
		io.ConfigViewportsNoDefaultParent = false;
		io.ConfigViewportsNoAutoMerge = true;
		io.IniFilename = configFilename.c_str();

		if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
			return Result(ResultStatus::Error, "Failed to initialize ImGui GLFW implementation.");

		if (!ImGui_ImplOpenGL3_Init("#version 150"))
			return Result(ResultStatus::Error, "Failed to initialize ImGui OpenGL implementation.");

		setBaseTheme(BaseTheme::DARK);

		return Result::Ok();
	}

	void ImGuiManager::setBaseTheme(BaseTheme theme)
	{
		ImGuiStyle* style = &ImGui::GetStyle();
		style->FramePadding.x = 4;
		style->FramePadding.y = 2;
		style->ItemSpacing.x = 2;
		style->ItemSpacing.y = 4;
		style->WindowPadding.x = 6;
		style->WindowRounding = 4;
		style->WindowBorderSize = 1;
		style->FrameBorderSize = 0;
		style->FrameRounding = 1;
		style->ScrollbarRounding = 6;
		style->ChildRounding = 2;
		style->PopupRounding = 2;
		style->GrabRounding = 1;
		style->TabRounding = 1;
		style->ScrollbarSize = 12;
		style->GrabMinSize = 8;

		ImVec4* colors = style->Colors;

		if (theme == BaseTheme::LIGHT)
		{
			ImGui::StyleColorsLight();
			colors[ImGuiCol_Text] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
			colors[ImGuiCol_WindowBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.00f);
			colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.80f, 0.85f);
			colors[ImGuiCol_BorderShadow] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
			colors[ImGuiCol_FrameBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
			colors[ImGuiCol_FrameBgHovered] = ImVec4(0.85f, 0.85f, 0.85f, 0.78f);
			colors[ImGuiCol_FrameBgActive] = ImVec4(0.85f, 0.85f, 0.85f, 0.67f);
			colors[ImGuiCol_TitleBgActive] = ImVec4(0.9f, 0.9f, 0.9f, 1.00f);
			colors[ImGuiCol_MenuBarBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
			colors[ImGuiCol_CheckMark] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
			colors[ImGuiCol_Button] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
			colors[ImGuiCol_ButtonHovered] = ImVec4(0.7f, 0.7f, 0.7f, 1.00f);
			colors[ImGuiCol_ButtonActive] = ImVec4(0.66f, 0.84f, 0.95f, 1.00f);
			colors[ImGuiCol_PlotHistogram] = ImVec4(0.66f, 0.84f, 0.95f, 1.00f);
			colors[ImGuiCol_Header] = ImVec4(0.97f, 0.97f, 0.97f, 1.00f);
			colors[ImGuiCol_HeaderHovered] = ImVec4(0.97f, 0.97f, 0.97f, 1.00f);
			colors[ImGuiCol_HeaderActive] = ImVec4(0.97f, 0.97f, 0.97f, 1.00f);
			colors[ImGuiCol_Separator] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
			colors[ImGuiCol_SeparatorActive] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
			colors[ImGuiCol_Tab] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
			colors[ImGuiCol_TabHovered] = ImVec4(0.99f, 0.48f, 0.88f, 0.80f);
			colors[ImGuiCol_TabActive] = ImVec4(0.16f, 0.44f, 0.75f, 1.00f);
			colors[ImGuiCol_TabUnfocused] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
			colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
			colors[ImGuiCol_ChildBg] = ImVec4(0.9f, 0.9f, 0.9f, 1.00f);
			colors[ImGuiCol_PopupBg] = ImVec4(0.9f, 0.9f, 0.9f, 1.00f);
			colors[ImGuiCol_TitleBg] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
			colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
			colors[ImGuiCol_ScrollbarBg] = ImVec4(0.9f, 0.9f, 0.9f, 1.00f);
			colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
			colors[ImGuiCol_TableHeaderBg] = ImVec4(0.9f, 0.9f, 0.9f, 1.00f);
			colors[ImGuiCol_TableRowBg] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
			colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
		}
		else
		{
			ImGui::StyleColorsDark();
			colors[ImGuiCol_WindowBg] = ImVec4(0.196f, 0.196f, 0.196f, 1.00f);
			colors[ImGuiCol_Border] = ImVec4(0.10f, 0.10f, 0.10f, 0.85f);
			colors[ImGuiCol_BorderShadow] = ImVec4(0.10f, 0.10f, 0.10f, 0.35f);
			colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
			colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.15f, 0.78f);
			colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 0.67f);
			colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
			colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
			colors[ImGuiCol_CheckMark] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
			colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
			colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
			colors[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.44f, 0.75f, 1.00f);
			colors[ImGuiCol_PlotHistogram] = ImVec4(0.16f, 0.44f, 0.75f, 1.00f);
			colors[ImGuiCol_Header] = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
			colors[ImGuiCol_HeaderHovered] = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
			colors[ImGuiCol_HeaderActive] = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
			colors[ImGuiCol_Separator] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
			colors[ImGuiCol_SeparatorActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
			colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
			colors[ImGuiCol_TabHovered] = ImVec4(0.19f, 0.48f, 0.88f, 0.80f);
			colors[ImGuiCol_TabActive] = ImVec4(0.16f, 0.44f, 0.75f, 1.00f);
			colors[ImGuiCol_TabUnfocused] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
			colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
			colors[ImGuiCol_ChildBg] = ImVec4(0.196f, 0.196f, 0.196f, 1.00f);
			colors[ImGuiCol_PopupBg] = ImVec4(0.196f, 0.196f, 0.196f, 1.00f);
			colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
			colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
			colors[ImGuiCol_ScrollbarBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
			colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
			colors[ImGuiCol_TableHeaderBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
			colors[ImGuiCol_TableRowBg] = ImVec4(0.2f, 0.2f, 0.2f, 1.00f);
			colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
		}

		this->theme = theme;
		applyAccentColor(config.accentColor);
	}

	void ImGuiManager::shutdown()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiManager::begin()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		float dpiScale = ImGui::GetMainViewport()->DpiScale;
		if (dpiScale != styleScale)
		{
			ImGui::GetStyle().ScaleAllSizes(dpiScale / styleScale);
			styleScale = dpiScale;
		}
	}

	void ImGuiManager::draw(GLFWwindow* window)
	{
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(window);
		}
	}

	void ImGuiManager::loadFont(const std::string& filename, float size)
	{
		if (!IO::File::exists(filename))
			return;

		static ImFontConfig fontConfig{};
		fontConfig.PixelSnapH = true;
		fontConfig.OversampleH = 1;
		fontConfig.RasterizerMultiply = 1.05f;

		ImFontGlyphRangesBuilder rangeBuilder;
		static ImVector<ImWchar> ranges;
		rangeBuilder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesDefault());
		rangeBuilder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesJapanese());
		rangeBuilder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesKorean());
		rangeBuilder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
		rangeBuilder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesVietnamese());
		rangeBuilder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());
		// rangeBuilder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesThai());
		// rangeBuilder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesGreek());
		rangeBuilder.BuildRanges(&ranges);

		auto font = ImGui::GetIO().Fonts->AddFontFromFileTTF(filename.c_str(), (int)size,
		                                                     &fontConfig, ranges.Data);
		ImGui::GetIO().Fonts->Build();
	}

	void ImGuiManager::loadIconFont(const std::string& filename, int start, int end, float size)
	{
		if (!IO::File::exists(filename))
			return;

		static ImFontConfig fontConfig{};
		fontConfig.MergeMode = true;
		fontConfig.GlyphMinAdvanceX = 13.0f;
		fontConfig.PixelSnapH = false;
		fontConfig.OversampleH = 1;
		static const ImWchar iconRanges[] = { start, end, 0 };
		ImGui::GetIO().Fonts->AddFontFromFileTTF(filename.c_str(), (int)size, &fontConfig,
		                                         iconRanges);
	}

	void ImGuiManager::buildFonts(float dpiScale)
	{
		// clear existing fonts on rebuild
		ImGuiIO& io = ImGui::GetIO();
		if (!io.Fonts->Fonts.empty())
			io.Fonts->Clear();

		io.FontDefault = nullptr;

		loadFont(Application::getAppDir() + "res/fonts/NotoSansCJK-Regular.ttc", 16 * dpiScale);
		loadIconFont(Application::getAppDir() + "res/fonts/fa-solid-900.ttf", ICON_MIN_FA,
		             ICON_MAX_FA, 12 * dpiScale);
		ImGui_ImplOpenGL3_CreateFontsTexture();
	}

	void ImGuiManager::initializeLayout()
	{
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
		                               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		                               ImGuiWindowFlags_NoMove |
		                               ImGuiWindowFlags_NoBringToFrontOnFocus |
		                               ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImVec2 viewportOffset{ 0, UI::toolbarBtnSize.y + ImGui::GetStyle().WindowPadding.y + 5 };
		ImGui::SetNextWindowPos(viewport->WorkPos + viewportOffset);
		ImGui::SetNextWindowSize(viewport->WorkSize - viewportOffset);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("InvisibleWindow", nullptr,
		             windowFlags); // This is basically the background window that contains all the
		                           // dockable windows
		ImGui::PopStyleVar(3);

		std::string dockStrId{ "InvisibleWindowDockSpace" };

		ImGuiID dockSpaceId = ImGui::GetID(dockStrId.c_str());
		if (!ImGui::DockBuilderGetNode(dockSpaceId))
		{
			ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockSpaceId, viewport->WorkSize - viewportOffset);

			ImGuiID dockMainId = dockSpaceId;
			ImGuiID midRightId = ImGui::DockBuilderSplitNode(dockMainId, ImGuiDir_Right, 0.25f,
			                                                 nullptr, &dockMainId);
			ImGuiID bottomRightId =
			    ImGui::DockBuilderSplitNode(midRightId, ImGuiDir_Down, 0.3f, nullptr, &midRightId);

			ImGui::DockBuilderDockWindow("###notes_timeline", dockMainId);
			ImGui::DockBuilderDockWindow("###chart_properties", midRightId);
			ImGui::DockBuilderDockWindow("###note_properties", midRightId);
			ImGui::DockBuilderDockWindow("###options", bottomRightId);
			ImGui::DockBuilderDockWindow("###presets", bottomRightId);
			ImGui::DockBuilderDockWindow("###layers", bottomRightId);
			ImGui::DockBuilderDockWindow("###waypoints", bottomRightId);

			ImGui::DockBuilderFinish(dockMainId);
		}

		ImGui::DockSpace(dockSpaceId, ImVec2(), ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::End();
	}

	void ImGuiManager::applyAccentColor(int colIndex)
	{
		ImVec4* colors = ImGui::GetStyle().Colors;
		const ImVec4 color = UI::accentColors[colIndex];
		const ImVec4 darkColor = generateDarkColor(color);
		const ImVec4 lightColor = generateHighlightColor(color);

		colors[ImGuiCol_SliderGrab] = color;
		colors[ImGuiCol_SliderGrabActive] = darkColor;
		colors[ImGuiCol_ButtonActive] = darkColor;
		colors[ImGuiCol_SeparatorHovered] = lightColor;
		colors[ImGuiCol_TabHovered] = lightColor;
		colors[ImGuiCol_TabActive] = color;
		colors[ImGuiCol_CheckMark] = color;
		colors[ImGuiCol_PlotHistogram] = color;
		colors[ImGuiCol_PlotHistogramHovered] = darkColor;

		accentColor = colIndex;
	}
}
