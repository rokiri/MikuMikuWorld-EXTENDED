#pragma once
#include "ChartGalleryWindow.h"
#include "ScoreEditorWindows.h"
#include "ScoreSerializeWindow.h"
#include "ScorePreview.h" // Å© Ç±ÇÍÇí«â¡
#include <future>

namespace MikuMikuWorld
{
	class ScoreEditor
	{
	  private:
		ScoreContext context{};
		EditArgs edit{};
		std::unique_ptr<Renderer> renderer;
		PresetManager presetManager;

		ScoreEditorTimeline timeline{};
		ScorePreviewWindow previewWindow{}; // Å© Ç±ÇÍÇí«â¡
		ScorePropertiesWindow propertiesWindow{};
		ScoreNotePropertiesWindow notePropertiesWindow{};
		ScoreOptionsWindow optionsWindow{};
		PresetsWindow presetsWindow{};
		DebugWindow debugWindow{};
		LayersWindow layersWindow{};
		WaypointsWindow waypointsWindow{};
		SettingsWindow settingsWindow{};
		ChartGalleryWindow galleryWindow;
		RecentFileNotFoundDialog recentFileNotFoundDialog{};
		AboutDialog aboutDialog{};
		UpdateAvailableDialog updateAvailableDialog{};
		ScoreSerializeWindow serializeWindow{};

		Stopwatch autoSaveTimer;
		std::string autoSavePath;
		bool showImGuiDemoWindow;

		bool save(std::string filename);

		void fetchUpdate();

	  public:
		ScoreEditor();

		void update();

		void create();
		void open();
		void loadScore(std::string filename);
		void loadMusic(std::string filename);
		size_t updateRecentFilesList(const std::string& entry);
		void exportScore();
		bool saveAs();
		bool trySave(std::string);
		void autoSave();
		int deleteOldAutoSave(int count);

		void drawMenubar();
		void drawToolbar();
		void help();

		void straightenHold3D(); // Å© Ç±Ç±Ç…í«â¡ÅF3Díºê¸âªä÷êîÇÃêÈåæ

		inline void loadPresets(std::string path) { presetManager.loadPresets(path); }
		inline void savePresets(std::string path) { presetManager.savePresets(path); }

		void writeSettings();
		void uninitialize();
		inline std::string_view getWorkingFilename() const { return context.workingData.filename; }
		constexpr inline bool isUpToDate() const { return context.upToDate; }
	};
}
