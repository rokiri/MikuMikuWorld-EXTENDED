#include "Application.h"
#include "Colors.h"
#include "IO.h"
#include "Localization.h"
#include "Utilities.h"
#include <json.hpp>
#include <stdexcept>
#include <thread>

namespace MikuMikuWorld
{
	Application* Application::instance;

	Application::Application() : initialized{ false }
	{
		language = "";
		version = "";
		instance = this;
		window = nullptr;
	}

	Result Application::initialize(const std::string& root)
	{
		if (initialized)
			return Result(ResultStatus::Success, "App is already initialized");

		configRoot = IO::getConfigPath(root);
		resourceRoot = IO::getResourcePath(root);
		version = IO::getBuildVersion();

		readConfiguration();

		Result result = initOpenGL();
		if (!result.isOk())
			return result;

		imgui = std::make_unique<ImGuiManager>();
		result = imgui->initialize(window);
		if (!result.isOk())
			return result;

		IO::initializePlatform(windowState.platfromData, this);

		editor = std::make_unique<ScoreEditor>();
		editor->loadPresets(getConfigPath("library"));

		loadResources();

#ifndef DEBUG
		std::thread fetchUpdateThread(&ScoreEditor::fetchUpdate, editor.get());
		fetchUpdateThread.detach();
#endif

		initialized = true;
		return Result::Ok();
	}

	Application& Application::getInstance() { return *instance; }

	void* Application::getAppWindowHandle()
	{
		return instance && instance->initialized
		           ? IO::getWindowHandle(instance->windowState.platfromData)
		           : nullptr;
	}

	const std::string& Application::getAppVersion() { return version; }

	void Application::dispose()
	{
		if (initialized)
		{
			editor->uninitialize();
			IO::disposePlatform(windowState.platfromData, this);
			imgui->shutdown();
			glfwDestroyWindow(window);
			glfwTerminate();
		}
		initialized = false;
		instance = nullptr;
	}

	void Application::readConfiguration()
	{
		auto configPath = getConfigPath(APP_CONFIG_FILENAME);
		if (std::filesystem::exists(configPath))
		{
			std::ifstream configFile(configPath);
			nlohmann::json configJson;
			configFile >> configJson;
			configFile.close();
			configJson.get_to(config);
		}

		windowState.maximized = config.maximized;
		windowState.vsync = config.vsync;
		windowState.position = config.windowPos;
		windowState.size = config.windowSize;
		UI::accentColors[0] = config.userColor.toImVec4();
	}

	void Application::writeConfiguration()
	{
		config.maximized = windowState.maximized;
		config.vsync = windowState.vsync;
		config.windowPos = windowState.position;
		config.windowSize = windowState.size;
		config.userColor = Color::fromImVec4(UI::accentColors[0]);
		editor->writeSettings();

		// update to latest version
		version = ApplicationConfiguration::CONFIG_VERSION;
		nlohmann::json configJson = config;
		std::ofstream configFile(getConfigPath(APP_CONFIG_FILENAME));
		configFile << std::setw(4) << configJson;
		configFile.flush();
		configFile.close();
	}

	void Application::appendOpenFile(const FilePath& filepath)
	{
		if (!std::filesystem::is_regular_file(filepath))
			return;
		editor->appendOpenFile(filepath);
	}

	void Application::update()
	{
		if (config.language != language)
		{
			localization.scanLanguages();
			if (!localization.setLanguage(config.language))
			{
				config.language = "auto";
				localization.setLanguage("auto");
			}
			language = config.language;
		}

		float dpiX = 1.0f, dpiY = 1.0f;
		GLFWmonitor* mainMonitor = glfwGetPrimaryMonitor();
		if (mainMonitor)
		{
			glfwGetMonitorContentScale(mainMonitor, &dpiX, &dpiY);
		}

		float dpiScale = (dpiX + dpiY) * 0.5f;
		if (dpiScale != windowState.lastDpiScale)
		{
			UI::updateUIScaling(dpiScale);
			windowState.lastDpiScale = dpiScale;
		}

		imgui->begin();
		imgui->initializeLayout();

		if (windowState.closing && editor->close())
		{
			glfwSetWindowShouldClose(window, 1);
		}

		editor->update();

		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		imgui->draw(window);
		glfwSwapBuffers(window);
	}

	void Application::loadResources()
	{
		resource.loadShader("basic2d");
		resource.loadShader("basicMask");

		resource.noteResources.scanProfiles();
		resource.noteResources.load(config.notesSkin);
		resource.timelineResources.load();
		resource.backgroundResources.load();

		if (!localization.setLanguage(config.language))
		{
			config.language = "auto";
			localization.setLanguage("auto");
		}
		language = config.language;
	}

	void Application::setTitle(const std::string& name)
	{
		if (!window)
			return;
		glfwSetWindowTitle(window, IO::formatString("%s - %s", APP_NAME, name).c_str());
	}

	void Application::run()
	{
		glfwShowWindow(window);

		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			update();
		}

		editor->savePresets(getConfigPath("library"));
		writeConfiguration();
	}
}