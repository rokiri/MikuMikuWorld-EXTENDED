#pragma once
#include "UI.h"
#include "json.hpp"
#include "Math.h"
#include "InputConfiguration.h"

namespace MikuMikuWorld
{
	constexpr size_t maxRecentFilesEntries = 10;

	struct ApplicationConfiguration
	{
		constexpr static const char* CONFIG_VERSION{ "1.12.0" };

		std::string version;

		// session state
		Vector2 windowPos;
		Vector2 windowSize;
		bool maximized;
		bool vsync;
		bool showFPS;
		BaseTheme baseTheme;
		int accentColor;
		Color userColor;
		std::string language;
		int divisionType;
		int division;
		float zoom;

		// editor settings
		bool debugEnabled;
		bool matchTimelineSizeToWindow;
		bool matchNotesSizeToTimeline;
		bool drawBackground;
		bool drawWaveform;
		bool showTickInProperties;
		bool followCursorInPlayback;
		bool returnToLastSelectedTickOnPause;
		bool useSmoothScrolling;
		bool minifyOutput;
		bool autoSaveEnabled;
		int defaultExportFormat;
		int timelineWidth;
		int notesHeight;
		float laneOpacity;
		float backgroundBrightness;
		float smoothScrollingTime;
		float cursorPositionThreshold;
		float scrollSpeedNormal;
		float scrollSpeedShift;
		int autoSaveInterval;
		int autoSaveMaxCount;
		float masterVolume;
		float bgmVolume;
		float seVolume;
		int seProfileIndex;
		std::string backgroundImage;
		std::string notesSkin;

		// Miscellaneous
		std::chrono::system_clock::time_point lastUpdateCheck;
		std::string latestFetchAppVersion;

		InputConfiguration input;

		std::vector<std::string> recentFiles;

		ApplicationConfiguration();

		void restoreDefault();
	};

	ApplicationConfiguration& getConfig();
}