#include "Note.h"
#include "Constants.h"
#include "Score.h"
#include <algorithm>

namespace MikuMikuWorld
{
	void HoldNote::insertStep(Note& note, NoteCollection& notes, bool swaps, bool update)
	{
		note.holdID = ID;
		auto it = std::upper_bound(steps.begin(), steps.end(), note, StepIdComparer(notes));
		if (it == steps.begin())
		{
			Note& prevStart = notes.at(steps.front());
			if (swaps)
				swapNoteProperties(prevStart, note);
			else
			{
				prevStart.flag =
				    setFlag(prevStart.flag, NoteFlag::LongNote | NoteFlag::NonAttached, false);
				note.flag = setFlag(note.flag, NoteFlag::LongNote);
				if (hasFlag(note.flag, NoteFlag::Attached))
					note.flag = setFlag(note.flag, NoteFlag::NonAttached);
			}
			steps.insert(it, note.ID);
		}
		else if (it == steps.end())
		{
			Note& prevEnd = notes.at(steps.back());
			if (swaps)
				swapNoteProperties(prevEnd, note);
			else
			{
				prevEnd.flag =
				    setFlag(prevEnd.flag, NoteFlag::LongNote | NoteFlag::NonAttached, false);
				note.flag = setFlag(note.flag, NoteFlag::LongNote);
				if (hasFlag(note.flag, NoteFlag::Attached))
					note.flag = setFlag(note.flag, NoteFlag::NonAttached);
			}
			steps.insert(it, note.ID);
		}
		else
		{
			steps.insert(it, note.ID);
			note.flag = setFlag(note.flag, NoteFlag::LongNote | NoteFlag::NonAttached, false);
		}
		if (update)
		{
			updateJoints(notes);
			updateFading(notes);
		}
	}

	void HoldNote::sortSteps(NoteCollection& notes, bool swaps)
	{
		Note& prevStart = notes.at(steps.front());
		Note& prevEnd = notes.at(steps.back());
		prevStart.flag = setFlag(prevStart.flag, NoteFlag::LongNote | NoteFlag::NonAttached, false);
		prevEnd.flag = setFlag(prevEnd.flag, NoteFlag::LongNote | NoteFlag::NonAttached, false);

		std::stable_sort(steps.begin(), steps.end(), StepIdComparer(notes));
		Note& start = notes.at(steps.front());
		Note& end = notes.at(steps.back());
		if (swaps)
		{
			if (prevStart.ID != start.ID)
				swapNoteProperties(prevStart, start);
			if (prevEnd.ID != end.ID)
				swapNoteProperties(prevEnd, end);
		}
		if (std::find_if(separators.begin(), separators.end(),
		                 [ID = prevStart.ID](const HoldNoteStep& s)
		                 { return s.ID == ID; }) != separators.end())
			prevStart.flag = setFlag(prevStart.flag, NoteFlag::LongNote);
		if (std::find_if(separators.begin(), separators.end(),
		                 [ID = prevEnd.ID](const HoldNoteStep& s)
		                 { return s.ID == ID; }) != separators.end())
			prevEnd.flag = setFlag(prevEnd.flag, NoteFlag::LongNote);
		start.flag = setFlag(start.flag, NoteFlag::LongNote);
		end.flag = setFlag(end.flag, NoteFlag::LongNote);
		if (hasFlag(start.flag, NoteFlag::Attached))
			start.flag = setFlag(start.flag, NoteFlag::NonAttached);
		if (hasFlag(end.flag, NoteFlag::Attached))
			end.flag = setFlag(end.flag, NoteFlag::NonAttached);

		std::sort(separators.begin(), separators.end(), HoldStepComparer(notes));
		updateJoints(notes);
		updateFading(notes);
	}

	void HoldNote::updateJoints(NoteCollection& notes)
	{
		joints.clear();
		for (size_t i = 0; i < steps.size(); ++i)
			if (!notes.at(steps[i]).isAttached())
				joints.push_back(steps[i]);
	}

	void HoldNote::updateFading(NoteCollection& notes)
	{
		auto nextSeparatorIt = separators.begin();
		for (size_t frontIdx = 0, endIdx = 0; endIdx != steps.size() - 1; frontIdx = endIdx)
		{
			const HoldNoteStep& holdStep =
			    nextSeparatorIt == separators.begin() ? *this : *prev(nextSeparatorIt);
			if (nextSeparatorIt == separators.end())
				endIdx = steps.size() - 1;
			else
			{
				auto endStepIt =
				    std::find(steps.begin() + frontIdx, steps.end(), nextSeparatorIt->ID);
				endIdx = std::distance(steps.begin(), endStepIt);
				++nextSeparatorIt;
			}
			if (!holdStep.isGuide())
				continue;
			Note& startStep = notes.at(steps[frontIdx]);
			Note& endStep = notes.at(steps[endIdx]);
			for (size_t idx = frontIdx; idx <= endIdx; idx++)
			{
				Note& step = notes.at(steps[idx]);
				switch (holdStep.fadeType)
				{
				case FadeType::Out:
					step.guideAlpha = unlerp(endStep.tick, startStep.tick, step.tick);
					break;
				case FadeType::None:
					step.guideAlpha = 1;
					break;
				case FadeType::In:
					step.guideAlpha = unlerp(startStep.tick, endStep.tick, step.tick);
					break;
				case FadeType::Classic:
					step.guideAlpha = lerp(1.0f, 0.2f, float(idx - frontIdx) / (endIdx - frontIdx));
					break;
				default:
				case FadeType::Custom:
					break;
				}
			}
		}
	}

	const Note* HoldNote::jointBeforeStep(const Note& step, const NoteCollection& notes) const
	{
		auto cmp = StepIdComparer<std::less<>>(notes);
		auto it = std::lower_bound(joints.begin(), joints.end(), step, cmp);
		if (it == joints.end())
			return &notes.at(*std::prev(it));
		if (cmp(step, *it))
			return it != joints.begin() ? &notes.at(*std::prev(it)) : nullptr;
		else
			return &notes.at(*it);
	}

	const Note* HoldNote::jointAfterStep(const Note& step, const NoteCollection& notes) const
	{
		auto cmp = StepIdComparer<std::greater<>>(notes);
		auto it = std::lower_bound(joints.rbegin(), joints.rend(), step, cmp);
		if (it == joints.rend())
			return &notes.at(*std::prev(it));
		if (cmp(step, *it))
			return it != joints.rbegin() ? &notes.at(*std::prev(it)) : nullptr;
		else
			return &notes.at(*it);
	}

	HoldNoteStep& HoldNote::holdStepAt(const Note& step, const NoteCollection& notes)
	{
		auto it =
		    std::upper_bound(separators.begin(), separators.end(), step, HoldStepComparer(notes));
		if (it == separators.begin())
			return *this;
		else
			return *std::prev(it);
	}

	const HoldNoteStep& HoldNote::holdStepAt(const Note& step, const NoteCollection& notes) const
	{
		auto it =
		    std::upper_bound(separators.begin(), separators.end(), step, HoldStepComparer(notes));
		if (it == separators.begin())
			return *this;
		else
			return *std::prev(it);
	}

	void setNotePosition(Note& n1, const Note& n2)
	{
		n1.tick = n2.tick;
		n1.lane = n2.lane;
		n1.width = n2.width;
	}

	void swapNotePosition(Note& n1, Note& n2)
	{
		std::swap(n1.tick, n2.tick);
		std::swap(n1.lane, n2.lane);
		std::swap(n1.width, n2.width);
	}

	void swapNoteProperties(Note& n1, Note& n2)
	{
		std::swap(n1.type, n2.type);
		std::swap(n1.flag, n2.flag);
		std::swap(n1.flick, n2.flick);
		std::swap(n1.ease, n2.ease);
		std::swap(n1.guideAlpha, n2.guideAlpha);
	}

	std::string_view getNoteSE(const Note& note)
	{
		switch (note.type)
		{
		case NoteType::Tap:
			if (note.isFlick())
				return note.isCrit() ? SE_CRITICAL_FLICK : SE_FLICK;
			else if (note.isTrace())
				return note.isCrit() ? SE_CRITICAL_FRICTION : SE_FRICTION;
			else
				return note.isCrit() && !note.isHold() ? SE_CRITICAL_TAP : SE_PERFECT;
		case NoteType::Tick:
			return note.isCrit() ? SE_CRITICAL_TICK : SE_TICK;
		case NoteType::Damage:
			return SE_DAMAGE;
		default:
			return "";
		}
	}
}
