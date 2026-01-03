#ifdef _WIN32
#define NOMINMAX
#include "Windows.h"
#endif

#include "PlatformIO.h"
#include "IO.h"
#include "File.h"
#include "Application.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace fs = std::filesystem;

namespace IO
{
	static constexpr UINT_PTR operator""_id(const char* s, std::size_t sz) noexcept
	{
		UINT_PTR n = 0;
		for (size_t i = 0, m = std::min(sizeof(UINT_PTR), sz); i < m; i++)
		{
			n <<= 8;
			n += static_cast<unsigned char>(s[i]);
		}
		return n;
	}

	struct PlatformData
	{
		HWND windowHandle;
		UINT_PTR timerId{ "mikumiku"_id };
		WNDPROC defaultWndProc{};
	};

	void* getWindowHandle(PlatformData* data) { return data->windowHandle; }

	LRESULT CALLBACK wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	void initializePlatform(PlatformData*& data, MikuMikuWorld::Application* app)
	{
		data = new PlatformData;

		// Override the current GLFW/Imgui window procedure
		HWND hwnd = glfwGetWin32Window(app->getGlfwWindow());
		data->windowHandle = hwnd;
		data->defaultWndProc = (WNDPROC)::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)wndProc);
		::DragAcceptFiles(hwnd, TRUE);

		return;
	}

	void disposePlatform(PlatformData*& data, MikuMikuWorld::Application* app)
	{
		(WNDPROC)::SetWindowLongPtrW(data->windowHandle, GWLP_WNDPROC,
		                             (LONG_PTR)data->defaultWndProc);
		delete data;
		data = nullptr;
	}

	static LRESULT CALLBACK wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto& app = MikuMikuWorld::Application::getInstance();
		auto& windowState = app.getWindowState();
		auto& data = windowState.platfromData;

		switch (uMsg)
		{
		case WM_TIMER:
			// Due to glfw implementation, grabbing/resizing the window blocks the message queue
			// causing the whole application to stop responding. As workaround, we create a timer
			// that update fast to simulate a regular program loops
			if (windowState.windowDragging && wParam == data->timerId)
			{
				if (app.getGlfwWindow())
					app.update();

				return 0;
			}
			break;

		case WM_ENTERSIZEMOVE:
			// Register the timer to update our application
			windowState.windowDragging = true;
			data->timerId = ::SetTimer(hwnd, data->timerId, USER_TIMER_MINIMUM, nullptr);
			break;

		case WM_EXITSIZEMOVE:
			// Remove the timer
			windowState.windowDragging = false;
			::KillTimer(hwnd, data->timerId);
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
							// app.appendOpenFile(IO::wideToUtf8(wFilename.data()));
						}
					}
				}

				::DragFinish(dropHandle);
			}
			break;

		default:
			// we don't handle this message ourselves so delegate it to the original glfw window's
			// proc
			if (data->defaultWndProc)
				return ::CallWindowProcW(data->defaultWndProc, hwnd, uMsg, wParam, lParam);
		}
		return ::DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}

	MessageBoxResult messageBox(const std::string& title, const std::string& message,
	                            MessageBoxButtons buttons, MessageBoxIcon icon, void* parentWindow)
	{
		UINT flags = 0;
		switch (icon)
		{
		case MessageBoxIcon::Information:
			flags |= MB_ICONINFORMATION;
			break;
		case MessageBoxIcon::Warning:
			flags |= MB_ICONWARNING;
			break;
		case MessageBoxIcon::Error:
			flags |= MB_ICONERROR;
			break;
		case MessageBoxIcon::Question:
			flags |= MB_ICONQUESTION;
			break;
		}

		switch (buttons)
		{
		case MessageBoxButtons::Ok:
			flags |= MB_OK;
			break;
		case MessageBoxButtons::OkCancel:
			flags |= MB_OKCANCEL;
			break;
		case MessageBoxButtons::YesNo:
			flags |= MB_YESNO;
			break;
		case MessageBoxButtons::YesNoCancel:
			flags |= MB_YESNOCANCEL;
			break;
		}

		const int result =
		    MessageBoxExW(static_cast<HWND>(parentWindow), utf8ToWide(message).c_str(),
		                  utf8ToWide(title).c_str(), flags, 0);
		switch (result)
		{
		case IDABORT:
			return MessageBoxResult::Abort;
		case IDCANCEL:
			return MessageBoxResult::Cancel;
		case IDIGNORE:
			return MessageBoxResult::Ignore;
		case IDNO:
			return MessageBoxResult::No;
		case IDYES:
			return MessageBoxResult::Yes;
		case IDOK:
			return MessageBoxResult::Ok;
		default:
			return MessageBoxResult::None;
		}
	}

	FileDialogResult FileDialog::showFileDialog(DialogType type, DialogSelectType selectType)
	{
		std::wstring wTitle = utf8ToWide(title);

		OPENFILENAMEW ofn;
		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = reinterpret_cast<HWND>(parentWindowHandle);
		ofn.lpstrTitle = wTitle.c_str();
		ofn.nFilterIndex = filterIndex + 1;
		ofn.nFileOffset = 0;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_LONGNAMES | OFN_EXPLORER | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT |
		            OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

		std::wstring wDefaultExtension = utf8ToWide(defaultExtension);
		ofn.lpstrDefExt = wDefaultExtension.c_str();

		std::vector<std::wstring> ofnFilters;
		ofnFilters.reserve(filters.size());

		/*
		    since '\0' terminates the string,
		    we'll do a C# by using ' | ' then replacing it with '\0' when constructing the final
		   wide string
		*/
		std::string filtersCombined;
		for (const auto& filter : filters)
		{
			filtersCombined.append(filter.filterName)
			    .append(" (")
			    .append(filter.filterType)
			    .append(")|")
			    .append(filter.filterType)
			    .append("|");
		}

		std::wstring wFiltersCombined = utf8ToWide(filtersCombined);
		std::replace(wFiltersCombined.begin(), wFiltersCombined.end(), '|', '\0');
		ofn.lpstrFilter = wFiltersCombined.c_str();

		std::wstring wInputFilename = utf8ToWide(inputFilename);
		wchar_t ofnFilename[1024]{ 0 };

		// suppress return value not used warning
#pragma warning(suppress : 6031)
		lstrcpynW(ofnFilename, wInputFilename.c_str(), 1024);
		ofn.lpstrFile = ofnFilename;

		if (type == DialogType::Save)
		{
			ofn.Flags |= OFN_HIDEREADONLY;
			if (GetSaveFileNameW(&ofn))
			{
				outputFilename = wideToUtf8(ofn.lpstrFile);
			}
			else
			{
				// user canceled
				return FileDialogResult::Cancel;
			}
		}
		else if (GetOpenFileNameW(&ofn))
		{
			outputFilename = wideToUtf8(ofn.lpstrFile);
		}
		else
		{
			return FileDialogResult::Cancel;
		}

		return outputFilename.empty() ? FileDialogResult::Cancel : FileDialogResult::OK;
	}

	std::string getSystemLocale()
	{
		static_assert(std::is_same_v<WCHAR, std::wstring::value_type>);
		std::wstring localName(LOCALE_NAME_MAX_LENGTH, L'\0');
		int result = GetUserDefaultLocaleName(localName.data(), LOCALE_NAME_MAX_LENGTH);
		localName = localName.substr(0, localName.find_first_of(L"-"));
		return IO::wideToUtf8(localName);
	}

	std::string getBuildVersion()
	{
		WCHAR filename[1024];
		GetModuleFileNameW(NULL, filename, sizeof(filename) / sizeof(*filename));

		DWORD verHandle = 0;
		UINT size = 0;
		LPBYTE lpBuffer = NULL;
		DWORD verSize = GetFileVersionInfoSizeW(filename, NULL);

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

	std::vector<std::string> getCommandLineArgs()
	{
		int argc;
		LPWSTR* args;
		args = CommandLineToArgvW(GetCommandLineW(), &argc);
		std::vector<std::string> cmdArgs;
		cmdArgs.reserve((size_t)argc);
		std::transform(args, args + argc, std::back_inserter(cmdArgs), wideToUtf8);
		return cmdArgs;
	}

	std::filesystem::path getConfigPath(const std::string& app_root)
	{
		return stringToPath(app_root).parent_path();
	}

	std::filesystem::path getResourcePath(const std::string& app_root)
	{
		return stringToPath(app_root).parent_path() / "res";
	}

	std::string wideToUtf8(const std::wstring& str)
	{
		if (str.empty())
			return {};

		int wSize = static_cast<int>(str.size());
		int size = WideCharToMultiByte(CP_UTF8, 0, str.data(), wSize, NULL, 0, NULL, NULL);
		std::string result(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, str.data(), wSize, result.data(), size, NULL, NULL);

		return result;
	}

	std::wstring utf8ToWide(const std::string& str)
	{
		if (str.empty())
			return {};

		int size = static_cast<int>(str.size());
		int wSize = MultiByteToWideChar(CP_UTF8, 0, str.data(), size, NULL, 0);
		std::wstring wResult(wSize, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.data(), size, wResult.data(), wSize);

		return wResult;
	}

	fs::path stringToPath(const std::string& str) { return fs::path(utf8ToWide(str)); }
	fs::path stringToPath(const std::wstring& str) { return fs::path(str); }

	std::string toString(const std::filesystem::path& path) { return wideToUtf8(path); }
	std::wstring toWString(const std::filesystem::path& path) { return path; }
}
