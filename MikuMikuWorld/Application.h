#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include "ScoreEditor.h"
#include "ImGuiManager.h"
#include "ApplicationConfiguration.h"
#include "ApplicationResource.h"
#include "PlatformIO.h"

struct PlatformData;

namespace MikuMikuWorld
{
	struct WindowState
	{
		bool vsync = true;
		bool maximized = false;
		bool closing = false;
		bool windowDragging = false;
		bool resetLayout = false;
		float lastDpiScale = 0.0f;
		Vector2 position{};
		Vector2 size{};
		IO::PlatformData* platfromData = nullptr;
	};

	class Application
	{
	  private:
		static Application* instance;

		GLFWwindow* window;
		std::unique_ptr<ScoreEditor> editor;
		std::unique_ptr<ImGuiManager> imgui;
		// UnsavedChangesDialog unsavedChangesDialog;

		bool initialized;
		std::string language;
		std::string version;
		FilePath configRoot;
		FilePath resourceRoot;

		WindowState windowState;
		ApplicationConfiguration config;
		ApplicationResource resource;
		Localization localization;

		Result initOpenGL();

	  public:
		Application();

		Result initialize(const std::string& root);
		void run();
		void update();
		void readConfiguration();
		void writeConfiguration();
		void appendOpenFile(const FilePath& filepath);
		void loadResources();
		void dispose();

		inline GLFWwindow* getGlfwWindow() { return window; }
		inline WindowState& getWindowState() { return windowState; }

		const std::string& getAppVersion();

		template <typename... TPath> std::filesystem::path getResourcePath(TPath... paths)
		{
			return (resourceRoot / ... / paths);
		}

		template <typename... TPath> std::filesystem::path getConfigPath(TPath... paths)
		{
			return (configRoot / ... / paths);
		}

		static Application& getInstance();
		static void* getAppWindowHandle();

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
