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
		// editor->loadPresets(appDir + "library");

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

	// void Application::handlePendingOpenFiles()
	//{
	//	std::string scoreFile{};
	//	std::string musicFile{};

	//	for (auto it = pendingOpenFiles.rbegin(); it != pendingOpenFiles.rend(); ++it)
	//	{
	//		std::string extension = IO::File::getFileExtension(*it);
	//		std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

	//		if (ScoreSerializeController::isValidFormat(
	//		        ScoreSerializeController::toSerializeFormat(*it)))
	//			scoreFile = *it;
	//		else if (Audio::isSupportedFileFormat(extension))
	//			musicFile = *it;

	//		if (!scoreFile.empty() && !musicFile.empty())
	//			break;
	//	}

	//	if (!scoreFile.empty())
	//	{
	//		windowState.resetting = true;
	//		// pendingLoadScoreFile = scoreFile;
	//	}

	//	if (!musicFile.empty())
	//		editor->loadMusic(musicFile);

	//	pendingOpenFiles.clear();
	//	windowState.dragDropHandled = true;
	//}

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

		// if ((windowState.closing || windowState.resetting) && !editor->isUpToDate() &&
		//     !unsavedChangesDialog.open)
		//{
		//	unsavedChangesDialog.open = true;
		//	ImGui::OpenPopup(MODAL_TITLE("unsaved_changes"));
		// }

		// auto unsavedChangesResult = unsavedChangesDialog.update();

		if (windowState.closing)
		{
			glfwSetWindowShouldClose(window, 1);
			// if (!editor->isUpToDate())
			//{
			//	switch (unsavedChangesResult)
			//	{
			//	case DialogResult::Yes:
			//		editor->trySave(editor->getWorkingFilename().data());
			//		glfwSetWindowShouldClose(window, 1);
			//		break;

			//	case DialogResult::No:
			//		glfwSetWindowShouldClose(window, 1);
			//		break;

			//	case DialogResult::Cancel:
			//		windowState.closing = false;
			//		break;

			//	default:
			//		break;
			//	}
			//}
			// else
			//{
			//	glfwSetWindowShouldClose(window, 1);
			//}
		}

		// if (windowState.resetting)
		//{
		//	if (!editor->isUpToDate())
		//	{
		//		switch (unsavedChangesResult)
		//		{
		//		case DialogResult::Yes:
		//			editor->trySave(editor->getWorkingFilename().data());
		//			break;

		//		case DialogResult::Cancel:
		//			windowState.resetting = shouldPickScore = false;
		//			pendingLoadScoreFile.clear();
		//			break;

		//		default:
		//			break;
		//		}
		//	}

		//	// Already saved or clicked save changes or discard changes
		//	if (editor->isUpToDate() || (unsavedChangesResult != DialogResult::Cancel &&
		//	                             unsavedChangesResult != DialogResult::None))
		//	{
		//		if (windowState.shouldPickScore)
		//		{
		//			editor->open();
		//			windowState.shouldPickScore = false;
		//		}
		//		else if (pendingLoadScoreFile.size())
		//		{
		//			editor->loadScore(pendingLoadScoreFile);
		//			pendingLoadScoreFile.clear();
		//		}
		//		else
		//		{
		//			editor->create();
		//		}

		//		windowState.resetting = false;
		//	}
		//}

		editor->update();

		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		imgui->draw(window);
		glfwSwapBuffers(window);
	}

	void Application::loadResources()
	{
		resource.loadShader("basic2d");

		resource.noteTexture.scanProfiles();
		resource.noteTexture.load(config.notesSkin);

		resource.timelineTexture.load();

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

		// editor->savePresets(appDir + "library");
		writeConfiguration();
	}
}