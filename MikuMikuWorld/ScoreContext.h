#pragma once
#include "Audio/AudioManager.h"
#include "Audio/Waveform.h"
#include "Constants.h"
#include "HistoryManager.h"
#include "Jacket.h"
#include "JsonIO.h"
#include "Score.h"
#include "ScoreStats.h"
#include "PreviewData.h"
#include "Tempo.h"
#include <unordered_set>
#include <atomic>

namespace MikuMikuWorld
{
	enum class TimeDivisionType : uint8_t
	{
		Quarter,
		Measure,
		DivisionTypeCount
	};
	enum class SnapMode : uint8_t
	{
		Relative,
		Absolute,
		IndividualAbsolute,
		SnapModeMax
	};
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

	struct EditArgs
	{
		float noteWidth{ 3 };
		float noteAlign{ 0.5f };
		FlickType flickType{ FlickType::Default };
		EaseType easeType{ EaseType::Linear };
		SoundEffectType soundEffect{ SoundEffectType::Default };
		EditHoldJointType startType{ EditHoldJointType::Normal };
		EditHoldStepType stepType{ EditHoldStepType::Normal };
		EditHoldJointType endType{ EditHoldJointType::Normal };
		GuideColor colorType{ GuideColor::Green };
		FadeType fadeType{ FadeType::Out };
		HoldStepLayer holdLayer{ HoldStepLayer::Top };
		InsertMode insertMode{ InsertMode::Select };

		float bpm{ 160.0f };
		int timeSignatureNumerator{ 4 };
		int timeSignatureDenominator{ 4 };
		float hiSpeed{ 1.0f };
		float hiSpeedSkip{ 0.0f };
		HiSpeedEaseType hiSpeedEase{ HiSpeedEaseType::None };
		bool hiSpeedHideNotes{ false };

		bool drawHoldStepOutlines{ true };

		void changeInsertMode(InsertMode newMode);
		bool isNoteInsertMode() const;
	};

	struct PasteData : public NotesContext
	{
		NoteOrderedCollection notesOrderedView;
		HiSpeedCollection hiSpeedChanges;
		bool pasting{ false };

		float width{};
		float minLane{};

		void cancelPaste();
		void startPaste();
		void load(const nlohmann::json& data);
		void updatePasteSize();
		void flip();
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
		CanEase = 1 << 8,
		HasGuideAlphaNote = 1 << 9,
		CanSetGuideAlphaNote = 1 << 10,
		CanConnectHold = 1 << 11,
		CanSoundEffect = 1 << 12,
		DirtyProperty = 1 << 15,
	};

	struct ScoreContext
	{
		static constexpr id_t LAYER_ALL = -1;

		Score score;
		ScoreMetadata metadata;
		std::string filename;
		ScoreStats scoreStats;
		HistoryManager history;
		std::unique_ptr<Audio::AudioContext> audio;
		Audio::WaveformMipChain waveformL, waveformR;

		bool upToDate{ true };
		bool showAllLayers = false;
		SelectionFlag selectedFlag = SelectionFlag::None;

		bool isPendingLoadMusic{ false };
		std::string pendingLoadMusicFilename{};
		std::unique_ptr<std::atomic_bool> isMusicLoading =
		    std::make_unique<std::atomic_bool>(false);

		int recentHistoryUndo = 0;
		id_t nextNoteID = 0;
		id_t nextHoldID = 0;

		tick_t currentTick{ 0 };
		Engine::DrawData scorePreviewDrawData{};

		inline secs_t getTimeAtCurrentTick() const
		{
			return accumulateDuration(currentTick, score.tempoChanges);
		}

		id_t selectedLayer = 0;
		NoteViewCollection selectedNotes;
		HiSpeedRefCollection selectedHiSpeedChanges;
		std::vector<Note*> hoveringNotes;
		NoteOrderedCollection notesOrderedView;
		WaypointOrderedCollection waypointOrderedView;

		float minNoteWidth() const noexcept;
		float maxNoteWidth() const noexcept;
		float maxNoteWidth(float lane) const noexcept;
		float minLane() const noexcept;
		float maxLane() const noexcept;
		float maxLane(float width) const noexcept;
		EaseType maxEase() const noexcept;
		FlickType maxFlick() const noexcept;

		void setStep(EditHoldStepType step);
		void setFlick(FlickType flick);
		void setEase(EaseType ease);
		void setSoundEffect(SoundEffectType sound);
		void setFadeType(FadeType fade);
		void setGuideColor(GuideColor color);
		void setLayer(int layer);
		void setCriticals(int critical = true);
		void setCriticalHold(int critical = true);
		void setFriction(int friction = true);
		void setDummy(int dummy = true);
		void setDummyHold(int dummy = true);
		void setGuideAlpha(float alpha);
		void setHoldLayer(HoldStepLayer layer);
		void setHoldSeparator(int separator = true);

		void updateSelectionFlag();

		bool hasAnySelected() const;
		bool hasAnyNoteSelected() const;
		bool hasAnyHispeedSelected() const;
		bool hasNoteSelected(id_t noteID) const;
		bool hasNoteSelected(const Note& note) const;
		bool hasHispeedSelected(const HiSpeed& hispeed) const;
		tick_t getMinTickFromSelection() const;

		void selectNote(Note& note, bool update = true);
		void selectHiSpeed(const HiSpeed& hispeed);
		void deselectNote(const Note& note);
		void deselectHiSpeed(const HiSpeed& hispeed);
		void selectAll(id_t layer = LAYER_ALL);
		void deselectAll();

		void deleteSelection();
		void flipSelection();
		void cutSelection();
		void copySelection() const;
		void paste(PasteData& pasteData, float offsetLane, tick_t offsetTick, id_t holdID = -1);
		void shrinkSelection(tick_t spacing);
		void compressSelection();
		bool canMoveNoteSelection(tick_t& ticks, int quarterDivision, float& lanes,
		                          float laneDivision, SnapMode snapMode);
		void moveNoteSelection(tick_t ticks, int quarterDivision, float lanes, float laneDivision,
		                       SnapMode snapMode, bool update = true);
		void setPosNoteSelection(tick_t tick, float lanes, bool update = true);

		void connectHoldsInSelection();
		void splitHoldInSelection();
		void convertHoldToTraces(int quarterDivision, bool deleteHold, bool update = true);

		void lerpHiSpeeds(int quarterDivision, EaseType ease);

		void convertHoldToGuide(GuideColor color);
		void convertGuideToHold(bool critical);
		void convertHoldToNone();

		void updateViews();
		void undo();
		void redo();
		void pushHistory(std::string_view description);

		Note* insertNote(const Note& note, id_t holdID = -1, bool update = true);
		void eraseNote(Note& note, bool update = true);
		std::tuple<HoldNote&, Note&, Note&> insertHold(const Note& startNote, const Note& endNote,
		                                               const HoldNote& hold, bool update = true);
		void eraseHold(HoldNote& hold, bool update = true);
		id_t connectHolds(id_t currHoldID, id_t nextHoldID, bool update = true);
		std::pair<id_t, id_t> splitHoldAt(HoldNote& hold, size_t index, bool update = true);
		void insertTempoChange(const Tempo& tempo);
		void insertTimeSignature(const TimeSignature& timeSig);
		HiSpeed& insertHispeedChange(const HiSpeed& hispeed, bool update = true);
		Waypoint& insertWaypoint(tick_t tick, const std::string& name);
		void eraseWaypoint(id_t waypointID);
		void insertSkill(tick_t tick);

		void setLaneExtension(int value, bool update = true);
		void setScoreExtension(bool isExtended);

		bool isLayerVisible(id_t layer) const;
		bool isLayerInteractive(id_t layer) const;
		bool isLayerSelected(id_t layer) const;
	};
}
