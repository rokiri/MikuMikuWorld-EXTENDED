#pragma once
#include <set>
#include <vector>
#include <map>
#include "Constants.h"
#include "NoteTypes.h"

namespace MikuMikuWorld
{
	struct TimeSignature
	{
		measure_t measure = 0;
		int numerator = 4;
		int denominator = 4;
	};
	using TimeSignatureCollection = std::map<measure_t, TimeSignature>;

	struct Tempo
	{
		tick_t tick = 0;
		float quarterPerMinute = 120;
	};
	using TempoCollection = std::map<tick_t, Tempo>;

	struct Skill
	{
		tick_t tick;
		inline operator tick_t() const { return tick; }
	};

	using SkillTriggerCollection = std::set<Skill>;

	struct Fever
	{
		tick_t startTick = -1;
		tick_t endTick = -1;
	};

	struct HiSpeed
	{
		tick_t tick = 0;
		id_t layer = 0;
		float speed = 1.0f;
		float skips = 0.0f;
		HiSpeedEaseType ease = HiSpeedEaseType::None;
		bool hideNotes = false;
	};
	using HiSpeedCollection = std::map<tick_t, HiSpeed>;
	using HiSpeedRefCollection = std::set<layered_tick_t>;

	// This is purely a helper and should not be use as a comparator
	bool isSame(const HiSpeed&, const HiSpeed&);
}