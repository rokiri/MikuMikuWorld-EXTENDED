#pragma once
#include "UI.h"
#include <string>
#include <memory>

struct GLFWwindow;
struct ImGuiStyle;

namespace MikuMikuWorld
{
	class Result;
	

	class ImGuiManager
	{
	  private:
		std::string configFilename{};

		BaseTheme theme{ BaseTheme::DARK };
		int accentColor{ 1 };
		float styleScale{ 1.0f };
		std::unique_ptr<ImGuiStyle> baseStyle;

		static void dockNodeSetFlag(ImGuiDockNode* node, ImGuiDockNodeFlags flag);

	  public:
		ImGuiManager();

		Result initialize(GLFWwindow* window);
		void initializeLayout();
		void shutdown();
		void begin();
		void loadFont(const std::string& filename, float size);
		void loadIconFont(const std::string& filename, int start, int end, float size);
		void buildFonts(float dpiScale = 1.0f);
		void draw(GLFWwindow* window);

		void setBaseTheme(BaseTheme theme);
		void applyAccentColor(int colIndex);

		inline constexpr BaseTheme getBaseTheme() const { return theme; }
		inline constexpr int getAccentColor() const { return accentColor; }
	};
}