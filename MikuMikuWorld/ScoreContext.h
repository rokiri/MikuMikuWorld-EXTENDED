#pragma once
#include "Audio/AudioManager.h"
#include "Audio/Waveform.h"
#include "Constants.h"
#include "HistoryManager.h"
#include "Jacket.h"
#include "JsonIO.h"
#include "Score.h"
#include "ScoreStats.h"
#include <unordered_set>

namespace MikuMikuWorld
{
	enum class InsertMode : uint8_t
	{
		Select,
		InsertTap,
		InsertLong,
		InsertLongMid,
		InsertFlick,
		MakeCritical,
		MakeFriction,
		InsertGuide,
		InsertDamage,
		MakeDummy,
		InsertBPM,
		InsertTimeSign,
		InsertHiSpeed,
		InsertModeMax
	};

	inline constexpr const char* insertModes[]{
		"timeline_select",  "timeline_tap",      "timeline_hold",  "timeline_hold_step",
		"timeline_flick",   "timeline_critical", "timeline_trace", "timeline_guide",
		"timeline_damage",  "timeline_dummy",    "timeline_bpm",   "timeline_time_signature",
		"timeline_hi_speed"
	};

	struct EditArgs
	{
		float noteWidth{ 3 };
		FlickType flickType{ FlickType::Default };
		EaseType easeType{ EaseType::Linear };
		EditHoldJointType startType{ EditHoldJointType::Normal };
		EditHoldStepType stepType{ EditHoldStepType::Normal };
		EditHoldJointType endType{ EditHoldJointType::Normal };
		GuideColor colorType{ GuideColor::Green };
		FadeType fadeType{ FadeType::Out };
		InsertMode insertMode{ InsertMode::Select };

		float bpm{ 160.0f };
		int timeSignatureNumerator{ 4 };
		int timeSignatureDenominator{ 4 };
		float hiSpeed{ 1.0f };
		float hiSpeedSkip{ 0.0f };
		HiSpeedEaseType hiSpeedEase{ HiSpeedEaseType::None };
		bool hiSpeedHideNotes{ false };

		void changeInsertMode(InsertMode newMode);
		bool isNoteInsertMode() const;
	};

	class EditorScoreData
	{
	  public:
		std::string title{};
		std::string designer{};
		std::string artist{};
		std::string filename{};
		std::string musicFilename{};
		float musicOffset{};
		int laneExtension{};
		Jacket jacket{};

		EditorScoreData() {}
		EditorScoreData(const ScoreMetadata& metadata, const std::string& filename)
		    : title{ metadata.title }, designer{ metadata.author }, artist{ metadata.artist },
		      musicFilename{ metadata.musicFile }, musicOffset{ metadata.musicOffset },
		      laneExtension{ metadata.laneExtension }
		{
			this->filename = filename;
			jacket.load(metadata.jacketFile);
		}

		ScoreMetadata toScoreMetadata() const
		{
			return { title, artist, designer, musicFilename, jacket.getFilename(), musicOffset, laneExtension };
		}
	};

	struct PasteData
	{
		std::unordered_map<id_t, Note> notes;
		std::unordered_map<id_t, HoldNote> holds;
		std::unordered_map<id_t, Note> damages;
		std::unordered_map<id_t, HiSpeedChange> hiSpeedChanges;
		bool pasting{ false };
		int offsetTicks{};
		int offsetLane{};
		int midLane{};
		int minLaneOffset{};
		int maxLaneOffset{};
	};

	enum class SelectionFlag : uint16_t
	{
		None,
		CanTrace = 1 << 0,
		CanCritical = 1 << 1,
		CanFlick = 1 << 2,
		CanDummy = 1 << 3,
		HasHoldNote = 1 << 4,
		HasGuideNote = 1 << 5,
		HasAnyHoldMid = 1 << 6,
		HasAnyHoldNoteStep = 1 << 7,
		CanEase = 1 << 8, // Any hold start + hold mid not attached
		CanChangeAlpha = 1 << 9,
		CanConnectHold = 1 << 10,
		// Flag for properties window
		DirtyProperty = 1 << 15,

	};

	struct ScoreContext
	{
		static constexpr id_t LAYER_ALL = -1;

		Score score;
		EditorScoreData workingData;
		ScoreMetadata metadata;
		std::string filename;
		ScoreStats scoreStats;
		HistoryManager history;
		Audio::AudioManager audio;
		Audio::WaveformMipChain waveformL, waveformR;

		bool upToDate{ true };
		bool showAllLayers = false;
		SelectionFlag selectedFlag = SelectionFlag::None;


		id_t nextNoteID = 0;
		id_t nextHoldID = 0;

		id_t selectedLayer = 0;
		NoteViewCollection selectedNotes;
		HiSpeedRefCollection selectedHiSpeedChanges;
		std::vector<Note*> hoveringNotes;
		NoteOrderedCollection notesOrderedView; // fast lookup
		WaypointOrderedCollection waypointOrderedView;

		int minNoteWidth() const noexcept;
		int maxNoteWidth() const noexcept;
		int maxNoteWidth(float lane) const noexcept;
		int minLane() const noexcept;
		int maxLane() const noexcept;
		int maxLane(float width) const noexcept;
		EaseType maxEase() const noexcept;
		FlickType maxFlick() const noexcept;

		void setStep(EditHoldStepType step);
		void setFlick(FlickType flick);
		void setEase(EaseType ease);
		void setFadeType(FadeType fade);
		void setGuideColor(GuideColor color);
		void setLayer(int layer);
		void setCriticals(int critical = true);
		void setCriticalHold(int critical = true);
		void setFriction(int friction = true);
		void setDummy(int dummy = true);
		void setDummyHold(int dummy = true);
		void setGuideAlpha(float alpha);

		void updateSelectionFlag();

		bool hasAnySelected() const;
		bool hasAnyNoteSelected() const;
		bool hasAnyHispeedSelected() const;
		bool hasNoteSelected(id_t noteID) const;
		bool hasNoteSelected(const Note& note) const;
		bool hasHispeedSelected(const HiSpeed& hispeed) const;
		tick_t getMinTickFromSelection() const;

		void selectNote(Note& note);
		void selectHiSpeed(const HiSpeed& hispeed);
		void deselectNote(const Note& note);
		void deselectHiSpeed(const HiSpeed& hispeed);
		void selectAll(id_t layer = LAYER_ALL);
		void deselectAll();

		void deleteSelection();
		void flipSelection();
		void cutSelection();
		void copySelection();
		void paste(bool flip);
		void duplicateSelection(bool flip);
		void doPasteData(const nlohmann::json& data, bool flip);
		void cancelPaste();
		void confirmPaste();
		void shrinkSelection(tick_t spacing);
		void compressSelection();

		void connectHoldsInSelection();
		void splitHoldInSelection();
		void repeatMidsInSelection();
		/**
		 * @brief Convert normal holds or guide notes within selection into traces
		 * @param division Current division. Used to determine the ticks between two trace notes
		 * @param deleteOrigin Delete the original hold notes or not
		 */
		void convertHoldToTraces(int quarterDivision, bool deleteHold, bool update = true);

		void lerpHiSpeeds(int quarterDivision, EaseType ease);

		void convertHoldToGuide(GuideColor color);
		void convertGuideToHold(bool critical);
		void convertHoldToNone();

		void updateViews();
		void undo();
		void redo();
		void pushHistory(std::string description, const Score& prev, const Score& current);

		Note* insertNote(const Note& note, id_t holdID = -1, bool update = true);
		// Will affect any view that referencing the note, like selection
		// Please make a copy if you need to erase them while iterating
		void eraseNote(Note& note, bool update = true);
		std::tuple<HoldNote&, Note&, Note&> insertHold(const Note& startNote, const Note& endNote,
		                                               const HoldNoteStep& step,
		                                               bool update = true);
		void eraseHold(HoldNote& hold, bool update = true);
		// Connects two holds together
		id_t connectHolds(id_t currHoldID, id_t nextHoldID, bool update = true);
		std::pair<id_t, id_t> splitHoldAt(HoldNote& hold, size_t index, bool update = true);
		void insertTempoChange(const Tempo& tempo);
		void insertTimeSignature(const TimeSignature& timeSig);
		HiSpeed& insertHispeedChange(const HiSpeed& hispeed, bool update = true);
		Waypoint& insertWaypoint(tick_t tick, const std::string& name);
		void eraseWaypoint(id_t waypointID);
		void insertSkill(tick_t tick);

		bool isLayerVisible(id_t layer) const;
		bool isLayerInteractive(id_t layer) const;
		bool isLayerSelected(id_t layer) const;
	};
}
