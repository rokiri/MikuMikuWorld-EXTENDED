#include "Utilities.h"
#include "Constants.h"
#include "ImGui/imgui.h"
#include "Localization.h"
#include "IO.h"
#include "Math.h"
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
		return IO::wideStringToMb(wL);
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
		return IO::formatString(getString("division"), div);
	}

	float Utilities::centerImGuiItem(const float width)
	{
		return (ImGui::GetContentRegionAvail().x * 0.5f) - (width * 0.5f);
	}

	void Utilities::ImGuiCenteredText(const std::string& str)
	{
		Utilities::ImGuiCenteredText(str.c_str());
	}

	void Utilities::ImGuiCenteredText(const char* str)
	{
		ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(str).x) *
		                     0.5f);
		ImGui::Text(str);
	}

	Random globalRandom{};

	float Random::get(float min, float max)
	{
		float factor = dist(gen);
		return lerp(min, max, factor);
	}

	float Random::get()
	{
		return dist(gen);
	}

	void Random::setSeed(int seed)
	{
		gen.seed(seed);
	}

	uint32_t RandN::xorShift()
	{
		uint32_t t = x ^ (x << 11);
		x = y;
		y = z;
		z = w;
		return w = w ^ (w >> 19) ^ t ^ (t >> 8);
	}

	uint32_t RandN::nextUInt32()
	{
		return xorShift();
	}

	float RandN::nextFloat()
	{
		return 1.0f - nextFloatRange(0.0f, 1.0f);
	}

	float RandN::nextFloatRange(float min, float max)
	{
		return (min - max) * (static_cast<float>(xorShift() << 9) / 0xFFFFFFFF) + max;
	}

	void RandN::setSeed(uint32_t seed)
	{
		x = seed;
		y = MT19937 * x + 1;
		z = MT19937 * y + 1;
		w = MT19937 * z + 1;
	}

	void drawShadedText(ImDrawList* drawList, ImVec2 textPos, float fontSize, ImU32 fontColor,
	                    const char* text)
	{
		if (!drawList)
			return;

		drawList->AddText(ImGui::GetFont(), fontSize, textPos + ImVec2{ 0.75f, 1.0f }, 0xff111111,
		                  text);
		drawList->AddText(ImGui::GetFont(), fontSize, textPos, fontColor, text);
	}
}
