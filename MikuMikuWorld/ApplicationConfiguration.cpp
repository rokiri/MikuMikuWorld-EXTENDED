#include "Application.h"
#include "ApplicationConfiguration.h"

using namespace nlohmann;

namespace MikuMikuWorld
{
	ApplicationConfiguration::ApplicationConfiguration() : version{ CONFIG_VERSION }
	{
		recentFiles.reserve(maxRecentFilesEntries);
		restoreDefault();
	}

	void ApplicationConfiguration::restoreDefault()
	{
		windowPos = Vector2(150, 100);
		windowSize = Vector2(1000, 800);
		maximized = false;
		vsync = true;
		accentColor = 1;
		userColor = Color(0.2f, 0.2f, 0.2f, 1.0f);
		language = "auto";

		minifyOutput = true;
		defaultExportFormat = -1;
		timelineWidth = 26;
		notesHeight = 26;
		notesSkin = "01";
		matchNotesSizeToTimeline = true;
		matchTimelineSizeToWindow = true;
		division = 2;
		zoom = 2.0f;
		laneOpacity = 0.6f;
		backgroundBrightness = 0.5f;
		drawBackground = true;
		backgroundImage = "";
		useSmoothScrolling = true;
		smoothScrollingTime = 48.0f;
		scrollSpeedNormal = 2.0f;
		scrollSpeedShift = 5.0f;
		cursorPositionThreshold = 0.5;
		drawWaveform = true;
		showTickInProperties = false;
		followCursorInPlayback = true;
		returnToLastSelectedTickOnPause = false;

		autoSaveEnabled = true;
		autoSaveInterval = 5;
		autoSaveMaxCount = 100;

		seProfileIndex = 0;
		masterVolume = 1.0f;
		bgmVolume = 1.0f;
		seVolume = 1.0f;

		debugEnabled = false;

		lastUpdateCheck = std::chrono::system_clock::time_point::min();
		latestFetchAppVersion = "0.0.0.0";
	}

	ApplicationConfiguration& getConfig() { return Application::instance->config; }
}
