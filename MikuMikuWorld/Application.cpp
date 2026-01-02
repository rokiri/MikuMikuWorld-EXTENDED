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
	std::string Application::version;
	std::string Application::appDir;

	Application::Application() : initialized{ false }
	{
		appDir = "";
		version = "";
		instance = this;
		window = nullptr;
	}

	Result Application::initialize(const std::string& root)
	{
		if (initialized)
			return Result(ResultStatus::Success, "App is already initialized");

		appDir = root;
		version = getVersion();
		language = "";

		readConfiguration();

		Result result = initOpenGL();
		if (!result.isOk())
			return result;

		imgui = std::make_unique<ImGuiManager>();
		result = imgui->initialize(window);
		if (!result.isOk())
			return result;

		// Override the current GLFW/Imgui window procedure
		HWND hwnd = glfwGetWin32Window(window);
		windowState.windowHandle = hwnd;
		windowState.defaultWndProc =
		    (WNDPROC)::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)wndProc);
		::DragAcceptFiles(hwnd, TRUE);

		editor = std::make_unique<ScoreEditor>();
		// editor->loadPresets(appDir + "library");

		loadResources();

		initialized = true;
		return Result::Ok();
	}

	Application& Application::getInstance() { return *instance; }

	void* Application::getAppWindowHandle()
	{
		return instance ? instance->windowState.windowHandle : nullptr;
	}

	const std::string& Application::getAppDir() { return appDir; }

	std::string Application::getVersion()
	{
		int argc;
		LPWSTR* args;
		args = CommandLineToArgvW(GetCommandLineW(), &argc);
		LPWSTR filename = args[0];

		DWORD verHandle = 0;
		UINT size = 0;
		LPBYTE lpBuffer = NULL;
		DWORD verSize = GetFileVersionInfoSizeW(filename, &verHandle);

		int major = 0, minor = 0, build = 0, rev = 0;
		if (verSize != NULL)
		{
			LPSTR verData = new char[verSize];

			if (GetFileVersionInfoW(filename, verHandle, verSize, verData))
			{
				if (VerQueryValue(verData, "\\", (VOID FAR * FAR*)&lpBuffer, &size))
				{
					if (size)
					{
						VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
						if (verInfo->dwSignature == 0xfeef04bd)
						{
							major = (verInfo->dwFileVersionMS >> 16) & 0xffff;
							minor = (verInfo->dwFileVersionMS >> 0) & 0xffff;
							rev = (verInfo->dwFileVersionLS >> 16) & 0xffff;
							build = (verInfo->dwFileVersionLS >> 0) & 0xffff;
						}
					}
				}
			}
			delete[] verData;
		}

		return IO::formatString("%d.%d.%d.%d", major, minor, rev, build);
	}

	const std::string& Application::getAppVersion() { return version; }

	void Application::dispose()
	{
		if (initialized)
		{
			editor->uninitialize();
			imgui->shutdown();
			glfwDestroyWindow(window);
			glfwTerminate();
		}
		initialized = false;
		instance = nullptr;
	}

	void Application::readConfiguration()
	{
		auto configPath = getFullPath(APP_CONFIG_FILENAME);
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
		std::ofstream configFile(appDir + APP_CONFIG_FILENAME);
		configFile << std::setw(4) << configJson;
		configFile.flush();
		configFile.close();
	}

	// void Application::appendOpenFile(const std::string& filename)
	//{
	//	pendingOpenFiles.push_back(filename);
	//	windowState.dragDropHandled = false;
	// }

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
			std::string locale =
			    config.language == "auto" ? Utilities::getSystemLocale() : config.language;

			// Try to set the selected language and fallback to default (en) on failure
			if (!Localization::setLanguage(locale))
				Localization::setLanguage("en");

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
			UI::updateBtnSizesDpiScaling(dpiScale);
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

		Localization::loadLanguages(appDir + "res\\i18n");
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

LRESULT CALLBACK wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	auto& app = MikuMikuWorld::Application::getInstance();
	auto& windowState = app.getWindowState();

	switch (uMsg)
	{
	case WM_TIMER:
		// Due to glfw implementation, grabbing/resizing the window blocks the message queue
		// causing the whole application to stop responding. As workaround, we create a timer
		// that update fast to simulate a regular program loops
		if (windowState.windowDragging && wParam == windowState.windowTimerId)
		{
			if (app.getGlfwWindow())
				app.update();

			return 0;
		}
		break;

	case WM_ENTERSIZEMOVE:
		// Register the timer to update our application
		windowState.windowDragging = true;
		windowState.windowTimerId =
		    ::SetTimer(hwnd, windowState.windowTimerId, USER_TIMER_MINIMUM, nullptr);
		break;

	case WM_EXITSIZEMOVE:
		// Remove the timer
		windowState.windowDragging = false;
		::KillTimer(hwnd, windowState.windowTimerId);
		break;

	case WM_DROPFILES:
		if (HDROP dropHandle = reinterpret_cast<HDROP>(wParam); dropHandle != NULL)
		{
			const UINT filesCount = ::DragQueryFileW(dropHandle, 0xFFFFFFFF, NULL, 0u);
			for (UINT i = 0; i < filesCount; ++i)
			{
				const UINT bufferSize = ::DragQueryFileW(dropHandle, i, NULL, 0u);
				if (bufferSize > 0)
				{
					std::wstring wFilename(bufferSize + 1, 0);
					if (::DragQueryFileW(dropHandle, i, wFilename.data(),
					                     static_cast<UINT>(wFilename.size())) != 0)
					{
						// app.appendOpenFile(IO::wideStringToMb(wFilename.data()));
					}
				}
			}

			::DragFinish(dropHandle);
		}
		break;

	default:
		// we don't handle this message ourselves so delegate it to the original glfw window's proc
		if (windowState.defaultWndProc)
			return ::CallWindowProcW(windowState.defaultWndProc, hwnd, uMsg, wParam, lParam);
	}
	return ::DefWindowProcW(hwnd, uMsg, wParam, lParam);
}