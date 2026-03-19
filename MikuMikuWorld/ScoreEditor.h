#pragma once
#include "ScoreEditorWindows.h"
#include "ScoreSerializeWindow.h"
#include "Rendering/Renderer.h"
#include "NoteResource.h"
#include <memory>
#include <future>

namespace MikuMikuWorld
{
	struct ScoreEditorState
	{
		bool wantCreateScore{ false };
		bool wantOpenScore{ false };
		bool wantExportScore{ false };
		bool wantSaveScore{ false };
		bool wantSaveAsScore{ false };
		bool wantClosing{ false };
		std::queue<FilePath> pendingOpenFiles;
		std::queue<std::pair<id_t, FilePath>> pendingExportTimelines;
		std::queue<id_t> pendingCloseTimelines;
		int closingTimelines{ 0 };
	};

	class ScoreEditor
	{
	  private:
		ScoreEditorState state;
		std::map<id_t, ScoreEditorTimeline> timelines;
		id_t currTimelineId{ 0 };
		id_t nextTimelineId{ 0 };
		ScoreEditorTimeline* currTimeline;
		ScoreContext* currContext;
		Audio::AudioManager audio;
		PresetManager presetManager;
		std::unique_ptr<Renderer> renderer;
		EditorToolbar toolbar;
		ScorePropertiesWindow propertiesWindow{};
		ScoreOptionsWindow optionsWindow;
		ScoreNotePropertiesWindow notePropertiesWindow{};
		EditArgs edit{};
		PasteData pasteData;
		PresetsWindow presetsWindow{};

		LayersWindow layersWindow{};

		SettingsWindow settingsWindow{};
		GenericDialog dialog;

		ScoreSerializeWindow serializeWindow{};

		Stopwatch autoSaveTimer;
		FilePath autoSavePath;
		bool showImGuiDemoWindow{ false };
		DebugWindow debugWindow{};
		WaypointsWindow waypointsWindow{};

		static void openHelp();
		static void openReleasePage();

	  public:
		ScoreEditor();

		void update();
		void handleEvents();

		void create();
		void open();
		// Notify the the editor should be closing, return whether the editor is ready to close
		bool close();
		void loadScore(std::string filename);
		void loadMusic(std::string filename);
		size_t updateRecentFilesList(const std::string& entry);
		void exportScore(ScoreEditorTimeline& timeline);
		bool saveAs(ScoreEditorTimeline& timeline);
		void autoSave();
		int deleteOldAutoSave(int count) const;
		void appendOpenFile(const FilePath& filepath);

		void drawMenubar();
		void fetchUpdate();

		void loadPresets(const FilePath& path);
		void savePresets(const FilePath& path);

		void writeSettings();
		void uninitialize();
	};
}
