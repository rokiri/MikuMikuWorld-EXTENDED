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
		constexpr static const char* CONFIG_VERSION{ "1.13.0" };

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
		bool hideStepOutlinesInPlayback;
		bool stopPlaybackAtMusicEnd;
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
		float zoomSensitivity;
		int autoSaveInterval;
		int autoSaveMaxCount;
		float masterVolume;
		float bgmVolume;
		float seVolume;
		std::string backgroundImage;
		std::string notesSkin;
		std::string seProfilePath;

		// Score Preview settings
		float pvNoteSpeed{ 8.0f };
		float pvBackgroundBrightness{ 0.5f };
		float pvStageOpacity{ 1.0f };
		float pvStageCover{ 0.0f };
		float pvHoldAlpha{ 0.85f };
		float pvGuideAlpha{ 0.7f };
		bool pvLockAspectRatio{ true };
		bool pvMirrorScore{ false };
		bool pvSimultaneousLine{ true };
		bool pvDrawToolbar{ true };
		bool pvLaneEffect{ true };
		bool pvNoteEffect{ true };
		bool pvNoteGlow{ true };
		bool pvFlickAnimation{ true };
		bool pvHoldAnimation{ true };

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