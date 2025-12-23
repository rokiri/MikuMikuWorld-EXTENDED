#pragma once

#define NOMINMAX
#include <Windows.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "ScoreEditor.h"
#include "ImGuiManager.h"

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
		std::unique_ptr<Renderer> renderer;
		UnsavedChangesDialog unsavedChangesDialog;

		bool initialized;
		std::string language;
		
		WindowState windowState;

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
		void readSettings();
		void writeSettings();
		void loadResources();
		void dispose();

		inline GLFWwindow* getGlfwWindow() { return window; }
		inline WindowState& getWindowState() { return windowState; }

		static Application& getInstance();
		static void* getAppWindowHandle();
		static const std::string& getAppDir();
		static const std::string& getAppVersion();
	};
}
