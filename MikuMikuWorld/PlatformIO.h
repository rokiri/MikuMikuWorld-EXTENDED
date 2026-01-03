#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

namespace MikuMikuWorld
{
	class Application;
}

namespace IO
{
	struct PlatformData;
	void* getWindowHandle(PlatformData* data);
	void initializePlatform(PlatformData*& data, MikuMikuWorld::Application* app);
	void disposePlatform(PlatformData*& data, MikuMikuWorld::Application* app);

	std::string getBuildVersion();
	std::vector<std::string> getCommandLineArgs();
	std::string getSystemLocale();
	
	std::filesystem::path getConfigPath(const std::string& app_root);
	std::filesystem::path getResourcePath(const std::string& app_root);

	std::filesystem::path stringToPath(const std::string& str);
	std::filesystem::path stringToPath(const std::wstring& str);

	std::string toString(const std::filesystem::path& path);
	std::wstring toWString(const std::filesystem::path& path);
}