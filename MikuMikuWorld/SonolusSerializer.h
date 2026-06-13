#pragma once
#include "ScoreSerializer.h"
#include "Sonolus.h"
#include "Constants.h"
#include <string>
#include <memory>

namespace MikuMikuWorld
{
	class SonolusEngine;
	class SonolusSerializer : public ScoreSerializer
	{
	  public:
		void serialize(const Score& score, std::string filename) override;
		Score deserialize(std::string filename) override;

		SonolusSerializer(std::unique_ptr<SonolusEngine>&& engine, bool compressData = true,
		                  bool prettyDump = false)
		    : engine(std::move(engine)), compressData(compressData), prettyDump(prettyDump)
		{
		}

	  private:
		std::unique_ptr<SonolusEngine> engine;
		bool compressData;
		bool prettyDump;
	};

	class SonolusEngine
	{
	  protected:
		using IntegerType = Sonolus::LevelDataEntity::IntegerType;
		using RealType = Sonolus::LevelDataEntity::RealType;
		using RefType = Sonolus::LevelDataEntity::RefType;
		using FuncRef = std::function<RefType(void)>;
		using TickType = decltype(Note::tick);
		using WidthType = decltype(Note::width);
		using LaneType = decltype(Note::lane);

		static double toBgmOffset(float musicOffset);
		static Sonolus::LevelDataEntity toBpmChangeEntity(const Tempo& tempo);
		static RealType ticksToBeats(TickType ticks, TickType beatTicks = TICKS_PER_BEAT);
		static RealType widthToSize(WidthType width);
		static RealType toSonolusLane(LaneType lane, WidthType width);

		static float fromBgmOffset(double bgmOffset);
		static bool fromBpmChangeEntity(const Sonolus::LevelDataEntity& bpmChangeEntity,
		                                Tempo& tempo);
		static TickType beatsToTicks(RealType beats, TickType beatTicks = TICKS_PER_BEAT);
		static WidthType sizeToWidth(RealType size);
		static LaneType toNativeLane(RealType lane, RealType size);

	  public:
		virtual Sonolus::LevelData serialize(const Score& score) = 0;
		virtual Score deserialize(const Sonolus::LevelData& levelData) = 0;
	};

	class PySekaiEngine : public SonolusEngine
	{
	  public:
		Sonolus::LevelData serialize(const Score& score) override;
		Score deserialize(const Sonolus::LevelData& levelData) override;

		static bool canSerialize(const Score& score);

	  private:
		static Sonolus::LevelDataEntity toGroupEntity(const Layer& layer);
		static Sonolus::LevelDataEntity toTimeScaleEntity(const HiSpeedChange& hispeed,
		                                                  const RefType& groupName);
		static Sonolus::LevelDataEntity toSkillEntity(const SkillTrigger& skill);
		static std::pair<Sonolus::LevelDataEntity, Sonolus::LevelDataEntity>
		toFeverEntities(const Fever& fever);
		static Sonolus::LevelDataEntity toNoteEntity(const Note& note, const std::string& archetype,
		                                             const RefType& groupName,
		                                             const HoldNote* hold = nullptr,
		                                             const HoldStep* holdStep = nullptr);
		static std::string getSingleNoteArchetype(const Note& note);
		static std::string getHoldNoteArchetype(const Note& note, bool isHead, bool isTail);
		static void insertTransientTickNote(size_t headIndex, size_t tailIndex, bool isActiveHead,
		                                    std::vector<Sonolus::LevelDataEntity>& entities);
		static void estimateAttachEntity(Sonolus::LevelDataEntity& attach,
		                                 const Sonolus::LevelDataEntity& head,
		                                 const Sonolus::LevelDataEntity& tail);
		static int toDirectionNumeric(FlickType flick);
		static int toEaseNumeric(EaseType ease);
		static int toSegmentNumeric(const HoldStep* holdStep = nullptr);

		static bool fromGroupEntity(const Sonolus::LevelDataEntity& groupEntity, Layer& layer);
		static bool fromTimeScaleEntity(const Sonolus::LevelDataEntity& timescaleEntity,
		                                const Sonolus::LevelDataEntity& groupEntity,
		                                HiSpeedChange& hispeed,
		                                std::unordered_map<RefType, size_t>& groupNameMap);
		static bool fromSkillEntity(const Sonolus::LevelDataEntity& skillEntity,
		                            SkillTrigger& skill);
		static bool fromFeverEntity(const Sonolus::LevelDataEntity& feverEntity, Fever& fever);

		static int fromNoteEntity(const Sonolus::LevelDataEntity& noteEntity, Note& note,
		                          const std::unordered_map<RefType, size_t>& groupNameMap);
		static int fromTapNoteEntity(const Sonolus::LevelDataEntity& tapNoteEntity, Note& note,
		                             const std::unordered_map<RefType, size_t>& groupNameMap);
		static int fromHoldMidEntity(const Sonolus::LevelDataEntity& noteEntity, Note& note,
		                             const std::unordered_map<RefType, size_t>& groupNameMap);

		static bool isValidNoteState(const Note& note);
		static bool isValidHoldNotes(const std::vector<Note>& holdNotes, const HoldNote& hold);

		static FlickType fromDirectionNumeric(int direction);
		static EaseType fromEaseNumeric(int ease);
		static bool fromSegmentNumeric(int segmentKind, HoldStep& holdStep);
	};
}
