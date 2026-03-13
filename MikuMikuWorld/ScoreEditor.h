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
		std::vector<FilePath> pendingOpenFiles;
		std::vector<id_t> pendingCloseTimelines;
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

		// ScoreSerializeWindow serializeWindow{};

		// Stopwatch autoSaveTimer;
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
		void close();
		void loadScore(std::string filename);
		void loadMusic(std::string filename);
		size_t updateRecentFilesList(const std::string& entry);
		void exportScore(ScoreContext& context);
		bool saveAs(ScoreContext& context);
		bool save(ScoreContext& context);
		void autoSave();
		int deleteOldAutoSave(int count);
		void appendOpenFile(const FilePath& filepath);

		void drawMenubar();
		void fetchUpdate();

		void loadPresets(const FilePath& path);
		void savePresets(const FilePath& path);

		void writeSettings();
		void uninitialize();
		// inline std::string_view getWorkingFilename() const { return context.workingData.filename; }
		
		// bool isUpToDate() const { return context.upToDate; }
	};
}
