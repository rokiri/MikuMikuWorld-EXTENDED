#include "Utilities.h"
#include "Constants.h"
#include "ImGui/imgui.h"
#include "Localization.h"
#include "IO.h"
#include "Text.h"
#include <Windows.h>
#include <ctime>
#include <vector>

namespace MikuMikuWorld
{
	std::string Utilities::getCurrentDateTime()
	{
		std::time_t now = std::time(0);
		std::tm localTime = *std::localtime(&now);

		char buf[128];
		strftime(buf, 128, "%Y-%m-%d-%H-%M-%S", &localTime);

		return buf;
	}

	std::string Utilities::getSystemLocale()
	{
		LPWSTR lpLocalName = new WCHAR[LOCALE_NAME_MAX_LENGTH];
		int result = GetUserDefaultLocaleName(lpLocalName, LOCALE_NAME_MAX_LENGTH);

		std::wstring wL = lpLocalName;
		wL = wL.substr(0, wL.find_first_of(L"-"));

		delete[] lpLocalName;
		return IO::wideToUtf8(wL);
	}

	std::vector<std::string> Utilities::splitString(const std::string& base, const char delimiter)
	{
		std::vector<std::string> result;
		std::string buffer;

		for (const auto& character : base)
		{
			if (character == delimiter)
			{
				result.push_back(buffer);
				buffer.clear();
			}
			else
			{
				buffer += character;
			}
		}

		result.push_back(buffer);

		return result;
	}

	std::string Utilities::getDivisionString(int div)
	{
		return IO::formatString(localize(Text::division), div);
	}
}
