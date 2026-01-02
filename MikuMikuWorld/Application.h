#pragma once

#define NOMINMAX
#include <Windows.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <filesystem>
#include "ScoreEditor.h"
#include "ImGuiManager.h"
#include "ApplicationConfiguration.h"
#include "ApplicationResource.h"

LRESULT CALLBACK wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

namespace MikuMikuWorld
{
	class Result;

	struct WindowState
	{
		bool vsync = true;
		bool maximized = false;
		bool closing = false;
		bool windowDragging = false;
		float lastDpiScale = 0.0f;
		void* windowHandle{};
		Vector2 position{};
		Vector2 size{};
		UINT_PTR windowTimerId{ 0x1234567887654321ull };
		WNDPROC defaultWndProc{};
	};

	class Application
	{
	  private:
		GLFWwindow* window;
		std::unique_ptr<ScoreEditor> editor;
		std::unique_ptr<ImGuiManager> imgui;
		// UnsavedChangesDialog unsavedChangesDialog;

		bool initialized;
		std::string language;

		WindowState windowState;
		ApplicationConfiguration config;
		ApplicationResource resource;
		Localization localization;

		static Application* instance;
		static std::string version;
		static std::string appDir;

		Result initOpenGL();
		std::string getVersion();

	  public:
		Application();

		Result initialize(const std::string& root);
		void run();
		void update();
		void readConfiguration();
		void writeConfiguration();
		void loadResources();
		void dispose();

		inline GLFWwindow* getGlfwWindow() { return window; }
		inline WindowState& getWindowState() { return windowState; }

		static Application& getInstance();
		static void* getAppWindowHandle();
		static const std::string& getAppDir();
		static const std::string& getAppVersion();

		template <typename... TPath> static std::filesystem::path getFullPath(TPath... paths)
		{
			return (std::filesystem::path(appDir) / ... / paths);
		}

		// Helpers
		// ApplicationConfig
		friend ApplicationConfiguration& getConfig();
		// ApplicationResource
		friend ApplicationResource& getResources();
		friend Shader* getShader(const std::string& name);
		// Localization
		friend const TranslationString& localize(const std::string& text);
	};
}
