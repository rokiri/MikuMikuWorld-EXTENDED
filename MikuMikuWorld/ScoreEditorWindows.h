#pragma once
#include "InputBinding.h"
#include "NotesPreset.h"
#include "ScoreEditorTimeline.h"
#include "Stopwatch.h"
#include <queue>
#include <mutex>

namespace MikuMikuWorld
{
	struct ScoreEditorState;
	class ScoreEditor;

	enum class DialogResult : uint8_t
	{
		None,
		Ok,
		Cancel,
		Yes,
		No,
		Retry
	};

	class EditorToolbar
	{
		InputBinding create;
		InputBinding open;
		InputBinding save;
		InputBinding exportScore;
		InputBinding cutSelection;
		InputBinding copySelection;
		InputBinding paste;
		InputBinding duplicate;
		InputBinding undo;
		InputBinding redo;
		InputBinding insertInputs[size_t(InsertMode::InsertModeMax)] = {};

		bool iconButton(const char* icon, std::string_view shortcutId, InputBinding& input,
		                const MultiInputBinding& shortcuts, bool enabled = true,
		                bool selected = false);
		bool imageButton(const Texture* texture, const Sprite* sprite, std::string_view txt,
		                 std::string_view shortcutId, InputBinding& input,
		                 const MultiInputBinding& shortcuts, bool enabled = true,
		                 bool selected = false);

		void updateIconBar(ScoreEditorState& state, ScoreContext* context, PasteData& pasteData);
		void updateEditBar(EditArgs& edit);

	  public:
		static constexpr const char* windowName = "(Main toolbar)###app_toolbar";
		void update(ScoreEditorState& state, ScoreContext* context, EditArgs& edit,
		            PasteData& pasteData);

	  private:
		int insertModePopup{};
	};

	struct DialogContent
	{
		using Callback = std::function<void()>;
		using Action = std::pair<std::string, Callback>;

		std::string title;
		std::vector<std::string> contents;
		std::vector<Action> actions;
	};

	// Basically a messagebox with ImGui UI
	// Reusable and avoid multiple dialogs from popup at the same time
	class GenericDialog
	{
		std::mutex contentMutex;
		std::queue<DialogContent> pendingDialogs;
		std::string currentName;

	  public:
		static constexpr const char* windowName = "###generic_dialog";
		void open(DialogContent content);
		void open(std::string title, std::vector<std::string> contents,
		          std::vector<DialogContent::Action> actions);
		void update();
	};

	class ScorePropertiesWindow
	{
	  public:
		static const char* getWindowName();
		void update(ScoreEditorTimeline& timeline, ScoreContext& context,
		            Audio::AudioManager& manager, GenericDialog& dialog);

	  private:
		std::string loadingText = "Loading...";
	};

	class ScoreNotePropertiesWindow
	{
		void updateState(const ScoreContext& context);

	  public:
		static const char* getWindowName();
		void update(ScoreContext& context);

	  private:
		tick_t tick;
		qnote_t quarter;
		id_t layer;
		float lane;
		float width;
		float speed;
		float skips;
		float alpha;
		NoteFlag noteFlag;
		FlickType flick;
		EaseType easeType;
		EditHoldStepType stepType;
		HoldNoteFlag holdFlag;
		GuideColor guideCol;
		FadeType fadeType;
		HiSpeedEaseType hspdEase;
		SoundEffectType soundEffect;
		HoldStepLayer holdLayer;
		bool hideNotes;
		bool holdSeparator;
		bool mixedTick, mixedLane, mixedWidth, mixedCritical, mixedTrace, mixedFlick, mixedDummy;
		bool mixedEase, mixedStep, mixedHoldCrit, mixedHoldDummy, mixedHoldSeparator, mixedGuideCol,
		    mixedFade, mixedAlpha, mixedSoundEffect, mixedHoldLayer;
		bool mixedLayer, mixedSpeed, mixedSkips, mixedhspdEase, mixedHideNotes;
	};

	class ScoreOptionsWindow
	{
		InputBinding increaseNoteSize;
		InputBinding decreaseNoteSize;

	  public:
		static const char* getWindowName();
		void update(ScoreContext& context, EditArgs& edit);
	};

	class PresetsWindow
	{
	  private:
		ImGuiTextFilter presetFilter;
		std::string presetName{};
		std::string presetDesc{};
		bool dialogOpen = false;

		DialogResult updateCreationDialog();

	  public:
		static const char* getWindowName();
		void update(PresetManager& presetManager, ScoreContext& context, PasteData& pasteData);
	};

	class SettingsWindow
	{
	  private:
		constexpr static int INPUT_TIMEOUT = 5;
		Stopwatch inputTimer;
		bool openPopup = false;
		bool listeningForInput = false;
		int editBindingIndex = -1;
		int selectedBindingIndex = 0;

		void updateGenericTab();
		void updateTimelineTab(Audio::AudioManager& audio);
		void updateKeyConfigTab();

	  public:
		bool isBackgroundChangePending = false;
		void open();
		DialogResult update(Audio::AudioManager& audio);
	};

	class UnsavedChangesDialog
	{
	  public:
		bool open = false;
		inline void close()
		{
			ImGui::CloseCurrentPopup();
			open = false;
		}

		DialogResult update();
	};

	class LayersWindow
	{
	  private:
		std::string popupModalName{};
		std::string editLayerName{};
		int renameIndex = -1;

		DialogResult updateDialog();
		static bool canLayerMerge(const Score& score, id_t index);
		static void doLayerMerge(ScoreContext& context, id_t index);
		static void doLayerHidden(ScoreContext& context, id_t index);
		static void doLayerMove(ScoreContext& context, id_t index, id_t offset);
		static void doLayerSwap(ScoreContext& context, id_t index, id_t newIndex);

	  public:
		static const char* getWindowName();

		void update(ScoreContext& context, GenericDialog& dialog);
	};

	class WaypointsWindow
	{
		measure_t gotoMeasure = 0;

	  public:
		static const char* getWindowName();

		void update(ScoreEditorTimeline& timeline);
	};

	class DebugWindow
	{
	  public:
		static const char* getWindowName();

		void update(Audio::AudioManager& audio);
	};
}
