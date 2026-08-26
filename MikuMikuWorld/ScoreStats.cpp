#include "ScoreStats.h"
#include "Score.h"
#include "Constants.h"
#include <algorithm>

namespace MikuMikuWorld
{
	ScoreStats::ScoreStats() { reset(); }

	void ScoreStats::reset()
	{
		resetCounts();
		resetCombo();
	}

	void ScoreStats::resetCounts()
	{
		hispeeds = 1;
		taps = flicks = holds = steps = guides = traces = total = 0;
	}

	void ScoreStats::resetCombo() { combo = 0; }

	void ScoreStats::calculateStats(const Score& score)
	{
		hispeeds = score.hiSpeedChanges.size();

		taps = std::count_if(score.notes.begin(), score.notes.end(),
		                     [](const auto& n)
		                     {
			                     const Note& note = n.second;
			                     return note.getType() == NoteType::Tap && !note.isFlick() &&
			                            !note.friction;
		                     });

		holds = std::count_if(score.holdNotes.begin(), score.holdNotes.end(),
		                      [](const auto& h) { return !h.second.isGuide(); });

		steps = std::count_if(score.notes.begin(), score.notes.end(), [](const auto& n)
		                      { return n.second.getType() == NoteType::HoldMid; });

		guides = std::count_if(score.holdNotes.begin(), score.holdNotes.end(),
		                       [](const auto& h) { return h.second.isGuide(); });

		flicks = std::count_if(score.notes.begin(), score.notes.end(),
		                       [](const auto& n) { return n.second.isFlick(); });

		traces = std::count_if(score.notes.begin(), score.notes.end(),
		                       [](const auto& n) { return n.second.friction; });

		total = score.notes.size();
		calculateCombo(score);
	}

	void ScoreStats::calculateCombo(const Score& score)
	{
		resetCombo();
		combo = score.notes.size();

		// Hold notes generate an extra combo tick every quarter beat (1/4),
		// not every eighth beat (1/8).
		constexpr int quarterBeat = TICKS_PER_BEAT;
		for (const auto& [id, note] : score.notes)
		{
			if (note.dummy)
				combo--;
		}
		for (const auto& [id, hold] : score.holdNotes)
		{
			if (hold.isGuide())
			{
				// Guide holds are not included
				combo -= 2 + hold.steps.size();
				continue;
			}

			// Hidden hold starts and ends do not count towards combo
			auto startIt = score.notes.find(hold.start.ID);
			auto endIt = score.notes.find(hold.end);
			if (startIt == score.notes.end() || endIt == score.notes.end())
				continue;

			if (!startIt->second.dummy && hold.startType != HoldNoteType::Normal)
				combo--;

			if (!endIt->second.dummy && hold.endType != HoldNoteType::Normal)
				combo--;

			combo -= std::count_if(hold.steps.begin(), hold.steps.end(), [](const HoldStep& step)
			                       { return step.type == HoldStepType::Hidden; });

			if (hold.dummy)
				continue;

			int startTick = startIt->second.tick;
			int endTick = endIt->second.tick;
			int quarterTick = startTick;

			quarterTick += quarterBeat;
			if (quarterTick % quarterBeat)
				quarterTick -= (quarterTick % quarterBeat);

			// hold <= 1/4th long
			if (quarterTick == startTick || quarterTick == endTick)
				continue;

			if (endTick % quarterBeat)
				endTick += quarterBeat - (endTick % quarterBeat);

			combo += (endTick - quarterTick) / quarterBeat;
		}
	}

	std::vector<ComboEvent> calculateComboEvents(const Score& score)
	{
		std::vector<ComboEvent> comboEvents;
		comboEvents.reserve(score.notes.size());

		for (const auto& [id, note] : score.notes)
		{
			if (note.dummy)
				continue;
			const HoldNote* hold = nullptr;
			const NoteType type = note.getType();

			if (type == NoteType::Hold)
			{
				const auto holdIt = score.holdNotes.find(note.ID);
				if (holdIt == score.holdNotes.end())
					continue;
				hold = &holdIt->second;
				if (hold->startType != HoldNoteType::Normal)
					continue;
			}
			else if (type == NoteType::HoldEnd)
			{
				const auto holdIt = score.holdNotes.find(note.parentID);
				if (holdIt == score.holdNotes.end())
					continue;
				hold = &holdIt->second;
				if (hold->endType != HoldNoteType::Normal)
					continue;
			}
			else if (type == NoteType::HoldMid)
			{
				const auto holdIt = score.holdNotes.find(note.parentID);
				if (holdIt == score.holdNotes.end())
					continue;
				hold = &holdIt->second;
				const int stepIndex = findHoldStep(*hold, note.ID);
				if (stepIndex != -1 && hold->steps[stepIndex].type == HoldStepType::Hidden)
					continue;
			}

			if (hold != nullptr && hold->isGuide())
				continue;

			float weight = 1.0f;
			if (note.isFlick() || note.friction || type == NoteType::HoldMid)
				weight = note.critical ? 0.2f : 0.1f;
			else
				weight = note.critical ? 2.0f : 1.0f;

			comboEvents.push_back({ note.tick, weight, true });
		}

		// Hold notes emit a scoring/combo tick every quarter beat (1/4),
		// not every eighth beat (1/8) — must match calculateCombo() above.
		constexpr int quarterBeat = TICKS_PER_BEAT;
		for (const auto& [holdId, hold] : score.holdNotes)
		{
			if (hold.isGuide() || hold.dummy)
				continue;
			const auto holdStartIt = score.notes.find(holdId);
			const auto holdEndIt = score.notes.find(hold.end);
			if (holdStartIt == score.notes.end() || holdEndIt == score.notes.end())
				continue;

			const Note& holdStart = holdStartIt->second;
			const int startTick = holdStart.tick;
			int endTick = holdEndIt->second.tick;
			int quarterTick = startTick + quarterBeat;
			if (quarterTick % quarterBeat)
				quarterTick -= quarterTick % quarterBeat;
			if (quarterTick == startTick || quarterTick == endTick)
				continue;
			if (endTick % quarterBeat)
				endTick += quarterBeat - (endTick % quarterBeat);

			for (int tick = quarterTick; tick < endTick; tick += quarterBeat)
				comboEvents.push_back({ tick, holdStart.critical ? 0.2f : 0.1f, false });
		}

		std::stable_sort(comboEvents.begin(), comboEvents.end(),
		                 [](const ComboEvent& a, const ComboEvent& b) { return a.tick < b.tick; });
		return comboEvents;
	}
}