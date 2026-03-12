#pragma once

#include <cstdint>
#include <climits>

namespace MikuMikuWorld
{
	constexpr int MAX_TICK = 100000000;
	constexpr int TICKS_PER_QUARTER = 480;
	constexpr float MAX_QUARTERS = MAX_TICK / TICKS_PER_QUARTER;
	constexpr int MAX_MEASURE = MAX_QUARTERS / 4;

	constexpr int MIN_TIME_SIGNATURE = 1;
	constexpr int MAX_TIME_SIGNATURE_NUMERATOR = 32;
	constexpr int MAX_TIME_SIGNATURE_DENOMINATOR = 64;
	constexpr float MIN_BPM = 10;
	constexpr float MAX_BPM = 10000;
	constexpr float MIN_HISPEED = 0.0001;
	constexpr float MAX_HISPEED = 10000;

	constexpr int UNDEFINED = -1;

	constexpr const char* NOTES_TEX = "notes1";
	constexpr const char* CC_NOTES_TEX = "notes2";
	constexpr const char* HOLD_PATH_TEX = "longNoteLine";
	constexpr const char* TOUCH_LINE_TEX = "touchLine_eff";
	constexpr const char* GUIDE_COLORS_TEX = "guideColors";
	constexpr const char* DUMMY_RED_CROSS = "notes3";
	constexpr const char* APP_CONFIG_FILENAME = "app_config.json";
	constexpr const char* IMGUI_CONFIG_FILENAME = "imgui_config.ini";

	constexpr const char* SUS_EXTENSION = ".sus";
	constexpr const char* USC_EXTENSION = ".usc";
	constexpr const char* JSON_EXTENSION = ".json";
	constexpr const char* GZ_JSON_EXTENSION = ".json.gz";
	constexpr const char* MMWS_EXTENSION = ".mmws";
	constexpr const char* CC_MMWS_EXTENSION = ".ccmmws";
	constexpr const char* UC_MMWS_EXTENSION = ".unchmmws";
}
