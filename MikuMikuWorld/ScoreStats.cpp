#include "ScoreStats.h"
#include "Score.h"
#include <functional>
#include <algorithm>
#include <numeric>

namespace MikuMikuWorld
{
	static bool isNormalHoldNote(const HoldNote& hold)
	{
		return !hasFlag(hold.flag,
		                HoldNoteFlag::Guide | HoldNoteFlag::Dummy);
	}

	template <typename TMapContainer, typename TFunc>
	static int getCount(const TMapContainer& m, TFunc f)
	{
		return std::count_if(std::begin(m), std::end(m), [&](const TMapContainer::value_type& e)
		                     { return std::invoke(f, e.second); });
	}

	ScoreStats::ScoreStats() { reset(); }

	void ScoreStats::reset()
	{
		taps = flicks = holds = guides = ticks = traces = damages = dummies = hispeeds = total =
		    combo = 0;
	}

	void ScoreStats::calculateStats(const Score& score)
	{
		taps =
		    getCount(score.notes, [](const Note& note)
		             { return note.type == NoteType::Tap && !note.isFlick() && !note.isTrace(); });

		holds = getCount(score.holdNotes, isNormalHoldNote);

		ticks = getCount(score.notes, [](const Note& n) { return n.type == NoteType::Tick; });

		guides = getCount(score.holdNotes, &HoldNote::isGuide);

		flicks = getCount(score.notes, &Note::isFlick);

		traces = getCount(score.notes, &Note::isTrace);

		damages = getCount(score.notes, [](const Note& n) { return n.type == NoteType::Damage; });

		dummies = getCount(score.notes, &Note::isDummy);

		hispeeds = std::accumulate(score.layers.begin(), score.layers.end(), size_t(0),
		                           [](size_t val, const Layer& layer)
		                           { return val + layer.hiSpeedChanges.size(); });

		total = score.notes.size();
		calculateCombo(score);
	}

	void ScoreStats::calculateCombo(const Score& score)
	{
		combo = score.notes.size();

		combo -= dummies;
		combo -= getCount(score.notes, [](const Note& n) { return n.isHidden() || n.isDummy(); });

		constexpr int eighthTicks = TICKS_PER_QUARTER / 2;
		for (const auto& [id, hold] : score.holdNotes)
		{
			if (hold.isGuide() || hold.isDummy())
				continue;

			int startTick = score.notes.at(hold.steps.front()).tick + eighthTicks;
			if (startTick % eighthTicks)
				startTick -= (startTick % eighthTicks);
			int endTick = score.notes.at(hold.steps.back()).tick;
			if (endTick % eighthTicks)
				endTick += eighthTicks - (endTick % eighthTicks);

			combo += (endTick - startTick) / eighthTicks;
		}
	}
}
