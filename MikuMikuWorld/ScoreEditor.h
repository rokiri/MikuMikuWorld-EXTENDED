#pragma once
#include "ScoreEditorWindows.h"
#include "ScoreSerializeWindow.h"
#include "Rendering/Renderer.h"
#include "NoteResource.h"
#include <memory>

namespace MikuMikuWorld
{
	struct ScoreEditorState
	{
		bool wantCreateScore{ false };
		bool wantOpenScore{ false };
		bool wantExportScore{ false };
		bool wantSaveScore{ false };
		std::vector<FilePath> pendingOpenFiles;
	};

	class ScoreEditor
	{
	  private:
		ScoreEditorState state;
		ScoreContext context{};
		// PresetManager presetManager;
		std::unique_ptr<Renderer> renderer;
		EditorToolbar toolbar;
		// ScoreEditorTimeline timeline{};
		// ScorePropertiesWindow propertiesWindow{};
		// ScoreNotePropertiesWindow notePropertiesWindow{};
		EditArgs edit{};
		//+ ScoreOptionsWindow optionsWindow{};
		//+ PresetsWindow presetsWindow{};
		// DebugWindow debugWindow{};
		// LayersWindow layersWindow{};
		// WaypointsWindow waypointsWindow{};
		// SettingsWindow settingsWindow{};
		// RecentFileNotFoundDialog recentFileNotFoundDialog{};
		// AboutDialog aboutDialog{};
		// UpdateAvailableDialog updateAvailableDialog{};
		// ScoreSerializeWindow serializeWindow{};

		// Stopwatch autoSaveTimer;
		FilePath autoSavePath;
		bool showImGuiDemoWindow{ false };

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
		void appendOpenFile(const FilePath& filepath);

		void drawMenubar();
		void help();

		// inline void loadPresets(std::string path) { presetManager.loadPresets(path); }
		// inline void savePresets(std::string path) { presetManager.savePresets(path); }

		void writeSettings();
		void uninitialize();
		// inline std::string_view getWorkingFilename() const { return context.workingData.filename; }
		
		// bool isUpToDate() const { return context.upToDate; }
	};
}
