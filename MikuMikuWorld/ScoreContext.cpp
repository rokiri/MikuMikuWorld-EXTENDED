#include "ScoreContext.h"
#include "Constants.h"
#include "IO.h"
#include "UI.h"
#include "Utilities.h"
#include "Math.h"
#include <cstdio>

using json = nlohmann::json;
using namespace IO;

/*
    NOTES: for ScoreContext
    Score and ScoreMetadata are not responsible for keeping the correctness of the score data
    It's the responsiblity of the functions that access/modify the score data to make sure the score
    remains correct when access/modify it (referably by going through ScoreContext)
    General correctness include:
    - Note:
        + Have width within minNoteWidth and maxNoteWidth
        + Have lane within minLane and maxLane(note.width)
        + Have holdID < 0 if note is not in a HoldNote
        + Not have flags that contradict eachother like NoteType::Tick & Friction
        + Have valid flick and ease type
        + Have 0 < guideAlpha < 1
    - HoldNote:
        + Have atleast 2 steps, and a start and end joint
        + Have steps and joints sorted in a given order
        + Start and end steps of a note chain must not be isAttached() == true
        + Not have flags that contradict eachother like Guide & Dummy

    NOTES: for ScoreMetadata::isExtendedScore
    This variable denotes whether the score can have extended features
    If isExtendedScore is false the following must be true
    - All notes that are not in a HoldNote:
        + Must be type NoteType::Tap
        + Must not be Hidden or Dummy (or Attached)
        + Layer = 0
        + FlickType && EaseType be valid and within support range
    - All hold notes:
        + Must not be Dummy
        + Must have any separators
        + First step and last Step must be NoteType::Tap and must not be Dummy (or Attached)
        + Every steps between first and last must be NoteType::Tick
        * For non Guide hold notes:
            + Crit must be the same as the first step
            + First step must have the FlickType::None
            + Every steps between first and last must have the same crit as the first
            + Last step must be of the same crit as the first
              unless the first step is not crit and the last step has friction or flick
            + At least 2 joints
        * For Guide hold notes:
            + All steps must be Hidden (and not Attached)
            + GuideColor can only be GuideColor::Green or GuideColor::Yellow
            + FadeType must be FadeType::Classic
            + Have steps = joints
    - All hispeed changes:
        + Must have skips = 0, ease = None, hideNotes = None
    - ScoreContext::selectedLayer must not change to anything other than 0
*/
namespace MikuMikuWorld
{
	constexpr const char* clipboardSignature = "MikuMikuWorld clipboard\n";
	static bool flip(bool v) { return !v; }
	static bool set(bool v) { return true; }
	static bool unset(bool v) { return false; }

	void EditArgs::changeInsertMode(InsertMode newMode)
	{
		if (insertMode != newMode)
		{
			insertMode = newMode;
		}
		else
		{
			switch (insertMode)
			{
			case InsertMode::InsertLong:
				easeType = cycleMode(easeType, EaseType::EaseTypeCount);
				break;
			case InsertMode::InsertLongMid:
				stepType = cycleMode(stepType, EditHoldStepType::HoldStepTypeCount);
				break;
			case InsertMode::InsertFlick:
				flickType = cycleMode(flickType, FlickType::FlickTypeCount);
				flickType = flickType != FlickType::None ? flickType : FlickType::Default;
				break;
			case InsertMode::InsertGuide:
				colorType = cycleMode(colorType, GuideColor::GuideColorCount);
				break;
			case InsertMode::InsertHiSpeed:
				hiSpeedHideNotes = !hiSpeedHideNotes;
				break;
			}
		}
	}

	bool EditArgs::isNoteInsertMode() const
	{
		return insertMode >= InsertMode::InsertTap && insertMode <= InsertMode::MakeDummy;
	}

	int ScoreContext::minNoteWidth() const noexcept { return metadata.isExtendedScore ? 0 : 1; }
	int ScoreContext::maxNoteWidth() const noexcept
	{
		return metadata.isExtendedScore ? 12 + metadata.laneExtension * 2 : 12;
	}

	int ScoreContext::maxNoteWidth(float lane) const noexcept
	{
		return (metadata.isExtendedScore ? 12 + metadata.laneExtension : 12) - lane;
	}

	int ScoreContext::minLane() const noexcept { return -metadata.laneExtension; }

	int ScoreContext::maxLane() const noexcept
	{
		return ScoreEditorTimeline::NUM_LANES + metadata.laneExtension;
	}

	int ScoreContext::maxLane(float width) const noexcept
	{
		return ScoreEditorTimeline::NUM_LANES + metadata.laneExtension - width;
	}

	EaseType ScoreContext::maxEase() const noexcept
	{
		return metadata.isExtendedScore ? EaseType::EaseTypeCount : EaseType::EaseInOut;
	}

	FlickType ScoreContext::maxFlick() const noexcept
	{
		return metadata.isExtendedScore ? FlickType::FlickTypeCount : FlickType::Down;
	}

	void ScoreContext::setStep(EditHoldStepType type)
	{
		if (!hasAnyNoteSelected())
			return;

		bool edit = false;
		Score prev = score;
		std::unordered_set<id_t> updatingHolds;
		for (auto&& [ID, pnote] : selectedNotes)
		{
			Note& note = *pnote;
			const HoldNote* hold = note.isHold() ? &score.holdNotes.at(note.holdID) : nullptr;
			if (!metadata.isExtendedScore &&
			    (!hold || hold->holdStepAt(note, score.notes).isGuide()))
				continue;

			EditHoldStepType curType = type;
			// Skip note must belong in a hold and not the first or last step in a hold
			bool cannotSetSkip = !hold || ID == hold->steps.front() || ID == hold->steps.back();
			if (curType == EditHoldStepType::HoldStepTypeCount)
			{
				// Cycle Note -> Hidden -> Attached
				if (!note.isAttached())
				{
					if (!note.isHidden())
						curType = EditHoldStepType::Hidden;
					else if (cannotSetSkip)
						curType = EditHoldStepType::Normal;
					else
						curType = EditHoldStepType::Skip;
				}
				else
				{
					curType = EditHoldStepType::Normal;
				}
			}

			switch (curType)
			{
			case EditHoldStepType::Normal:
				edit |= note.isHidden();
				note.flag = setFlag(note.flag, NoteFlag::Hidden | NoteFlag::NonAttached, false);
				if (note.isAttached())
				{
					edit = true;
					note.flag = setFlag(note.flag, NoteFlag::Attached, false);
					if (hold)
						updatingHolds.insert(hold->ID);
				}
				break;
			case EditHoldStepType::Hidden:
				note.flick = FlickType::None;
				note.flag = setFlag(note.flag, NoteFlag::Trace | NoteFlag::NonAttached, false);
				if (note.isAttached())
				{
					edit = true;
					note.flag = setFlag(note.flag, NoteFlag::Attached, false);
					if (hold)
						updatingHolds.insert(hold->ID);
				}
				edit |= !note.isHidden();
				note.flag = setFlag(note.flag, NoteFlag::Hidden);
				break;
			case EditHoldStepType::Skip:
				if (cannotSetSkip)
					note.flag = setFlag(note.flag, NoteFlag::NonAttached);
				edit = true;
				note.flag = setFlag(note.flag, NoteFlag::Hidden, false);
				note.flag = setFlag(note.flag, NoteFlag::Attached, true);
				if (hold)
					updatingHolds.insert(hold->ID);
				break;
			}
		}

		for (auto&& holdID : updatingHolds)
		{
			score.holdNotes.at(holdID).updateJoints(score.notes);
		}

		if (edit)
		{
			pushHistory("Change step type", prev, score);
			updateSelectionFlag();
		}
	}

	void ScoreContext::setFlick(FlickType flick)
	{
		if (!hasAnyNoteSelected())
			return;

		if (flick != FlickType::FlickTypeCount && flick >= maxFlick())
			return;

		bool edit = false;
		Score prev = score;
		for (auto&& [ID, pnote] : selectedNotes)
		{
			Note& note = *pnote;
			bool canFlick = note.canFlick();

			if (canFlick && note.isHold() && !metadata.isExtendedScore)
			{
				HoldNote& hold = score.holdNotes.at(note.holdID);
				canFlick = hold.steps.back() == ID;

				// Prevent critical hold end if the hold start is not critical
				if (canFlick && note.isCrit() && !note.isTrace() && !hold.isCrit() &&
				    cycleMode(note.flick, maxFlick()) == FlickType::None)
				{
					note.flag = setFlag(note.flag, NoteFlag::Critical, false);
				}
			}

			if (canFlick)
			{
				if (flick == FlickType::FlickTypeCount)
				{
					note.flick = cycleMode(note.flick, maxFlick());
					edit = true;
				}
				else
				{
					edit |= note.flick != flick;
					note.flick = flick;
				}
			}
		}

		if (edit)
		{
			pushHistory("Change flick", prev, score);
			updateSelectionFlag();
		}
	}

	void ScoreContext::setEase(EaseType ease)
	{
		if (!hasAnyNoteSelected())
			return;

		Score prev = score;
		if (ease != EaseType::EaseTypeCount && ease >= EaseType::EaseInOut &&
		    !metadata.isExtendedScore)
			return;

		bool edit = false;
		for (auto&& [_, pnote] : selectedNotes)
		{
			Note& note = *pnote;
			if (!note.hasEase())
				continue;
			if (ease == EaseType::EaseTypeCount)
			{
				edit = true;
				note.ease = cycleMode(note.ease, maxEase());
			}
			else
			{
				edit |= note.ease != ease;
				note.ease = ease;
			}
		}

		if (edit)
		{
			pushHistory("Change ease", prev, score);
			updateSelectionFlag();
		}
	}

	void ScoreContext::setFadeType(FadeType fade)
	{
		if (!hasAnyNoteSelected())
			return;

		Score prev = score;
		bool edit = false;
		std::unordered_set<HoldNoteStep*> updatedStep;
		for (auto&& [_, pnote] : selectedNotes)
		{
			Note& note = *pnote;
			if (!note.isHold())
				continue;
			HoldNote& hold = score.holdNotes.at(note.holdID);
			HoldNoteStep& step = hold.holdStepAt(note, score.notes);
			if (updatedStep.emplace(&step).second && step.fadeType != fade)
			{
				step.fadeType = fade;
				hold.updateFading(score.notes);
				edit = true;
			}
		}

		if (edit)
		{
			pushHistory("Change fade", prev, score);
			updateSelectionFlag();
		}
	}

	void ScoreContext::setGuideColor(GuideColor color)
	{
		if (!hasAnyNoteSelected())
			return;

		Score prev = score;
		bool edit = false;
		std::unordered_set<HoldNoteStep*> updatedStep;
		for (auto&& [_, pnote] : selectedNotes)
		{
			Note& note = *pnote;

			if (!note.isHold())
				continue;
			HoldNote& hold = score.holdNotes.at(note.holdID);
			HoldNoteStep& step = hold.holdStepAt(note, score.notes);
			if (!updatedStep.emplace(&step).second || !step.isGuide())
				continue;

			if (!metadata.isExtendedScore)
			{
				if (color == GuideColor::GuideColorCount)
				{
					edit = true;
					if (step.guideColor == GuideColor::Yellow)
						step.guideColor = GuideColor::Green;
					else
						step.guideColor = GuideColor::Yellow;
				}
				else if (color == GuideColor::Green || color == GuideColor::Yellow)
				{
					edit |= step.guideColor != color;
					step.guideColor = color;
				}
			}
			else
			{
				if (color == GuideColor::GuideColorCount)
				{
					edit = true;
					step.guideColor = cycleMode(step.guideColor, GuideColor::GuideColorCount);
				}
				else
				{
					edit |= step.guideColor != color;
					step.guideColor = color;
				}
			}
			hold.updateFading(score.notes);
		}

		if (edit)
		{
			pushHistory("Change guide", prev, score);
			updateSelectionFlag();
		}
	}

	void ScoreContext::setLayer(int layer)
	{
		if (!hasAnySelected())
			return;

		bool edit = false;
		Score prev = score;
		for (auto&& [_, pnote] : selectedNotes)
		{
			Note& note = *pnote;

			edit = note.layer != layer;
			note.layer = layer;
		}
		HiSpeedRefCollection newSelection;
		for (auto it = selectedHiSpeedChanges.begin(), end = selectedHiSpeedChanges.end();
		     it != end;)
		{
			auto&& [curLayer, tick] = *(it++);
			HiSpeedCollection& curCollection = score.layers[curLayer].hiSpeedChanges;
			HiSpeedCollection& newCollection = score.layers[layer].hiSpeedChanges;
			HiSpeed& hispeed = curCollection.at(tick);

			if (hispeed.layer != layer && newCollection.count(tick) == 0)
			{
				hispeed.layer = layer;
				newCollection.insert(curCollection.extract(tick));
			}
			newSelection.insert(newSelection.end(), { hispeed.layer, hispeed.tick });
		}
		selectedHiSpeedChanges = std::move(newSelection);

		if (edit)
		{
			pushHistory("Change layer", prev, score);
			updateSelectionFlag();
		}
	}

	void ScoreContext::setCriticals(int critical)
	{
		if (!hasAnyNoteSelected())
			return;
		bool (*setFunc)(bool) = critical < 0 ? flip : critical > 0 ? set : unset;

		bool edit = false;
		std::unordered_map<HoldNoteStep*, HoldNote*> holdSteps;
		for (auto&& [_, pnote] : selectedNotes)
		{
			Note& note = *pnote;
			if (note.isHold())
			{
				HoldNote& hold = score.holdNotes.at(note.holdID);
				HoldNoteStep& step = hold.holdStepAt(note, score.notes);
				holdSteps.emplace(&step, &hold);
			}
			else if (!note.canCrit())
				continue;
			bool oldState = note.isCrit();
			bool newState = setFunc(oldState);
			note.flag = setFlag(note.flag, NoteFlag::Critical, newState);
			edit |= newState != oldState;
		}

		for (auto& [pstep, phold] : holdSteps)
		{
			// flip critical state
			if (pstep->isGuide())
			{
				if (!metadata.isExtendedScore)
				{
					if (pstep->guideColor == GuideColor::Yellow)
					{
						pstep->guideColor = GuideColor::Green;
					}
					else
					{
						pstep->guideColor = GuideColor::Yellow;
					}
				}
			}
			else
			{
				id_t startID = pstep == phold ? phold->steps.front() : pstep->ID;
				auto stepIt = std::find(phold->steps.begin(), phold->steps.end(), startID);
				HoldNoteStep* nextSeperator;
				if (pstep == phold)
					nextSeperator = phold->separators.size() ? &phold->separators.front() : nullptr;
				else
				{
					auto nextSeperatorIt = std::next(std::find_if(
					    phold->separators.begin(), phold->separators.end(),
					    [ID = pstep->ID](const HoldNoteStep& s) { return s.ID == ID; }));
					nextSeperator =
					    nextSeperatorIt != phold->separators.end() ? &*nextSeperatorIt : nullptr;
				}
				auto endStepIt =
				    std::find(stepIt, phold->steps.end(), nextSeperator ? nextSeperator->ID : -1);
				if (!metadata.isExtendedScore)
				{
					// if the hold start is critical, every note in the hold must be critical
					Note& start = score.notes.at(startID);
					Note& end = score.notes.at(phold->steps.back());
					bool endSelected = hasNoteSelected(end);
					bool endSpecial = end.isTrace() || end.isFlick();
					bool isCrit = endSelected && endSpecial
					                  ? hasFlag(start.flag, NoteFlag::Critical)
					                  : setFunc(hasFlag(pstep->flag, HoldNoteFlag::Critical));
					bool flipEnd =
					    endSelected && endSpecial && hasFlag(end.flag, NoteFlag::Critical);

					pstep->flag = setFlag(pstep->flag, HoldNoteFlag::Critical, isCrit);
					for (; stepIt != endStepIt; ++stepIt)
					{
						Note& note = score.notes.at(*stepIt);
						note.flag = setFlag(note.flag, NoteFlag::Critical, isCrit);
					}
					if (flipEnd)
						end.flag |= NoteFlag::Critical;
				}
				else if (hasNoteSelected(startID))
				{
					Note& start = score.notes.at(startID);
					bool isCrit = hasFlag(start.flag, NoteFlag::Critical);
					pstep->flag = setFlag(pstep->flag, HoldNoteFlag::Critical, isCrit);
					for (; stepIt != endStepIt; ++stepIt)
					{
						Note& note = score.notes.at(*stepIt);
						note.flag = setFlag(note.flag, NoteFlag::Critical, isCrit);
					}
				}
			}
		}

		if (edit)
		{
			pushHistory("Change critical note");
			updateSelectionFlag();
		}
	}

	void ScoreContext::setCriticalHold(int critical)
	{
		if (!hasAnyNoteSelected())
			return;
		bool (*setFunc)(bool) = critical < 0 ? flip : critical > 0 ? set : unset;

		bool edit = false;
		std::unordered_map<HoldNoteStep*, HoldNote*> holdSteps;
		for (auto&& [_, pnote] : selectedNotes)
		{
			Note& note = *pnote;
			if (note.isHold())
			{
				HoldNote& hold = score.holdNotes.at(note.holdID);
				HoldNoteStep& step = hold.holdStepAt(note, score.notes);
				if (!step.isGuide())
					holdSteps.emplace(&step, &hold);
			}
		}

		for (auto& [pstep, phold] : holdSteps)
		{
			bool oldState = pstep->isCrit();
			bool newState = setFunc(oldState);
			pstep->flag = setFlag(pstep->flag, HoldNoteFlag::Critical, newState);
			edit |= newState != oldState;

			if (!metadata.isExtendedScore)
			{
				for (auto step : phold->steps)
				{
					Note& note = score.notes.at(step);
					note.flag = setFlag(note.flag, NoteFlag::Critical, newState);
				}
			}
		}

		if (edit)
		{
			pushHistory("Change critical hold");
			updateSelectionFlag();
		}
	}

	void ScoreContext::setFriction(int friction)
	{
		if (!hasAnyNoteSelected())
			return;
		bool (*setFunc)(bool) = friction < 0 ? flip : friction > 0 ? set : unset;

		bool edit = false;
		for (auto&& [_, pnote] : selectedNotes)
		{
			// Hold steps and invisible hold points cannot be trace notes
			Note& note = *pnote;
			if (note.type != NoteType::Tap)
				continue;

			if (note.isHold())
			{
				HoldNote& hold = score.holdNotes.at(note.holdID);

				if (note.type == NoteType::Tap && note.isHidden() &&
				    (metadata.isExtendedScore || !hold.isGuide()))
					note.flag = setFlag(note.flag, NoteFlag::Hidden, false);

				// Prevent critical hold end if the hold start is not critical
				if (!metadata.isExtendedScore && note.isCrit() && note.isTrace() &&
				    !note.isFlick() && note.ID == hold.steps.back() && !hold.isCrit())
					note.flag = setFlag(note.flag, NoteFlag::Critical, false);
			}

			if (!note.canTrace())
				continue;
			bool oldState = hasFlag(note.flag, NoteFlag::Trace);
			bool newState = setFunc(oldState);
			note.flag = setFlag(note.flag, NoteFlag::Trace, newState);
			edit |= oldState != newState;
		}

		if (edit)
		{
			pushHistory("Change trace notes", prev, score);
			updateSelectionFlag();
		}
	}

	void ScoreContext::setDummy(int dummy)
	{
		if (!hasAnyNoteSelected() || !metadata.isExtendedScore)
			return;
		bool (*setFunc)(bool) = dummy < 0 ? flip : dummy > 0 ? set : unset;

		Score prev = score;
		bool edit = false;
		for (auto&& [_, pnote] : selectedNotes)
		{
			Note& note = *pnote;
			if (!note.canDummy())
				continue;

			bool oldState = note.isDummy();
			bool newState = setFunc(oldState);
			note.flag = setFlag(note.flag, NoteFlag::Dummy, newState);
			edit |= oldState != newState;
		}

		if (edit)
		{
			pushHistory("Change dummy note", prev, score);
			updateSelectionFlag();
		}
	}

	void ScoreContext::setDummyHold(int dummy)
	{
		if (!hasAnyNoteSelected() || !metadata.isExtendedScore)
			return;
		bool (*setFunc)(bool) = dummy < 0 ? flip : dummy > 0 ? set : unset;

		bool edit = false;
		std::unordered_set<id_t> holds;
		for (auto&& [_, pnote] : selectedNotes)
		{
			Note& note = *pnote;
			if (!note.isHold())
				continue;
			holds.emplace(note.holdID);
		}
		for (auto&& holdID : holds)
		{
			HoldNote& hold = score.holdNotes.at(holdID);
			if (hold.isGuide())
				continue;
			bool oldState = hold.isDummy();
			bool newState = setFunc(oldState);
			hold.flag = setFlag(hold.flag, HoldNoteFlag::Dummy, newState);
			edit |= oldState != newState;
		}

		if (edit)
		{
			pushHistory("Change dummy hold");
			updateSelectionFlag();
		}
	}

	void ScoreContext::setGuideAlpha(float alpha)
	{
		if (!hasAnyNoteSelected() || !metadata.isExtendedScore)
			return;

		bool edit = false;
		std::unordered_set<id_t> holds;
		for (auto&& [_, pnote] : selectedNotes)
		{
			Note& note = *pnote;
			if (!note.isHold())
				continue;
			HoldNoteStep& step = score.holdNotes.at(note.holdID).holdStepAt(note, score.notes);
			if (!step.canSetAlpha())
				continue;
			edit = pnote->guideAlpha != alpha;
			pnote->guideAlpha = alpha;
		}

		if (edit)
		{
			pushHistory("Change guide alpha");
			updateSelectionFlag();
		}
	}

	void ScoreContext::updateSelectionFlag()
	{
		selectedFlag = setFlag(selectedFlag, SelectionFlag::DirtyProperty);
		auto begin = selectedNotes.begin(), end = selectedNotes.end();
		auto canFlick = [this](const NoteViewCollection::value_type& nv)
		{
			return nv.second->canFlick() &&
			       (metadata.isExtendedScore || !nv.second->isHold() ||
			        score.holdNotes.at(nv.second->holdID).steps.front() != nv.first);
		};
		auto hasEase = [this](const NoteViewCollection::value_type& nv)
		{
			if (!nv.second->hasEase())
				return false;
			const auto& joints = score.holdNotes.at(nv.second->holdID).joints;
			return std::find(joints.begin(), joints.end(), nv.first) != joints.end();
		};
		auto isHoldMid = [this](const NoteViewCollection::value_type& nv)
		{
			if (!nv.second->isHold())
				return false;
			const auto& steps = score.holdNotes.at(nv.second->holdID).steps;
			return nv.first != steps.front() && nv.first != steps.back();
		};
		auto isHoldNoteStep = [this](const NoteViewCollection::value_type& nv)
		{
			if (!nv.second->isHold())
				return false;
			const auto& hold = score.holdNotes.at(nv.second->holdID);
			if (hold.steps.front() == nv.first)
				return true;
			const auto& step = hold.holdStepAt(*nv.second, score.notes);
			return &step != &hold && step.ID == nv.first;
		};
		auto isNormalHold = [this](const NoteViewCollection::value_type& nv)
		{
			return nv.second->isHold() && !score.holdNotes.at(nv.second->holdID)
			                                   .holdStepAt(*nv.second, score.notes)
			                                   .isGuide();
		};
		auto isGuideHold = [this](const NoteViewCollection::value_type& nv)
		{
			return nv.second->isHold() && score.holdNotes.at(nv.second->holdID)
			                                  .holdStepAt(*nv.second, score.notes)
			                                  .isGuide();
		};
		auto hasCustomAlpha = [this](const NoteViewCollection::value_type& nv)
		{
			return nv.second->isHold() && score.holdNotes.at(nv.second->holdID)
			                                  .holdStepAt(*nv.second, score.notes)
			                                  .canSetAlpha();
		};
		auto canConnectHold = [this]()
		{
			if (selectedNotes.size() != 2)
				return false;
			const Note& n1 = *selectedNotes.begin()->second;
			const Note& n2 = *(++selectedNotes.begin())->second;
			if (!n1.isHold() || !n2.isHold())
				return false;
			const HoldNote& h1 = score.holdNotes.at(n1.holdID);
			const HoldNote& h2 = score.holdNotes.at(n2.holdID);
			if (n1.tick == n2.tick)
				return n1.ID == h1.steps.front() && n2.ID == h2.steps.back() ||
				       n2.ID == h2.steps.front() && n1.ID == h1.steps.back();
			else if (n1.tick < n2.tick)
				return n2.ID == h2.steps.front() && n1.ID == h1.steps.back();
			else
				return n1.ID == h1.steps.front() && n2.ID == h2.steps.back();
		};
		selectedFlag =
		    setFlag(selectedFlag, SelectionFlag::CanTrace,
		            std::any_of(begin, end, [](auto&& nv) { return nv.second->canTrace(); }));
		selectedFlag =
		    setFlag(selectedFlag, SelectionFlag::CanCritical,
		            std::any_of(begin, end, [](auto&& nv) { return nv.second->canCrit(); }));
		selectedFlag =
		    setFlag(selectedFlag, SelectionFlag::CanFlick, std::any_of(begin, end, canFlick));
		selectedFlag =
		    setFlag(selectedFlag, SelectionFlag::CanDummy,
		            metadata.isExtendedScore &&
		                std::any_of(begin, end, [](auto&& nv) { return nv.second->canDummy(); }));
		selectedFlag =
		    setFlag(selectedFlag, SelectionFlag::CanEase, std::any_of(begin, end, hasEase));
		selectedFlag =
		    setFlag(selectedFlag, SelectionFlag::HasAnyHoldMid, std::any_of(begin, end, isHoldMid));
		selectedFlag = setFlag(selectedFlag, SelectionFlag::HasAnyHoldNoteStep,
		                       std::any_of(begin, end, isHoldNoteStep));
		selectedFlag = setFlag(selectedFlag, SelectionFlag::HasHoldNote,
		                       std::any_of(begin, end, isNormalHold));
		selectedFlag = setFlag(selectedFlag, SelectionFlag::HasGuideNote,
		                       std::any_of(begin, end, isGuideHold));
		selectedFlag = setFlag(selectedFlag, SelectionFlag::CanChangeAlpha,
		                       hasFlag(selectedFlag, SelectionFlag::HasGuideNote) &&
		                           std::any_of(begin, end, hasCustomAlpha));
		selectedFlag = setFlag(selectedFlag, SelectionFlag::CanConnectHold, canConnectHold());
	}

	bool ScoreContext::hasAnySelected() const
	{
		return selectedNotes.size() || selectedHiSpeedChanges.size();
	}

	bool ScoreContext::hasAnyNoteSelected() const { return selectedNotes.size(); }

	bool ScoreContext::hasAnyHispeedSelected() const { return selectedHiSpeedChanges.size(); }

	bool ScoreContext::hasNoteSelected(id_t noteID) const { return selectedNotes.count(noteID); }

	bool ScoreContext::hasNoteSelected(const Note& note) const
	{
		return selectedNotes.count(note.ID);
	}

	bool ScoreContext::hasHispeedSelected(const HiSpeed& hispeed) const
	{
		return selectedHiSpeedChanges.count({ hispeed.layer, hispeed.tick });
	}

	tick_t ScoreContext::getMinTickFromSelection() const
	{
		using note_view_t = NoteViewCollection::value_type;
		using hispeed_ref_t = HiSpeedRefCollection::value_type;
		auto minNoteIt = std::min_element(selectedNotes.begin(), selectedNotes.end(),
		                                  [](const note_view_t& nv1, const note_view_t& nv2)
		                                  { return nv1.second->tick < nv2.second->tick; });
		tick_t minTick = minNoteIt == selectedNotes.end() ? MAX_TICK : minNoteIt->second->tick;

		auto hispeedComp = [&](const hispeed_ref_t& hs1, const hispeed_ref_t& hs2)
		{
			return score.layers[hs1.first].hiSpeedChanges.at(hs1.second).tick <
			       score.layers[hs2.first].hiSpeedChanges.at(hs2.second).tick;
		};
		auto minHspdIt = std::min_element(selectedHiSpeedChanges.begin(),
		                                  selectedHiSpeedChanges.end(), hispeedComp);
		if (minHspdIt == selectedHiSpeedChanges.end())
			return minTick;
		return std::min(score.layers[minHspdIt->first].hiSpeedChanges.at(minHspdIt->second).tick,
		                minTick);
	}

	void ScoreContext::selectNote(Note& note)
	{
		auto&& [_, emplaced] = selectedNotes.emplace(note.ID, &note);
		if (emplaced)
			updateSelectionFlag();
	}

	void ScoreContext::selectHiSpeed(const HiSpeed& hispeed)
	{
		selectedHiSpeedChanges.insert({ hispeed.layer, hispeed.tick });
	}

	void ScoreContext::deselectNote(const Note& note)
	{
		selectedNotes.erase(note.ID);
		updateSelectionFlag();
	}

	void ScoreContext::deselectHiSpeed(const HiSpeed& hispeed)
	{
		selectedHiSpeedChanges.erase({ hispeed.layer, hispeed.tick });
	}

	void ScoreContext::selectAll(id_t layer)
	{
		selectedNotes.clear();
		selectedHiSpeedChanges.clear();

		for (auto&& [ID, note] : score.notes)
			if (layer == LAYER_ALL || note.layer == layer)
				selectedNotes.emplace(ID, &note);

		for (id_t l = 0; l < score.layers.size(); ++l)
			if (layer == LAYER_ALL || l == layer)
				for (auto&& [tick, hispeed] : score.layers[l].hiSpeedChanges)
					selectedHiSpeedChanges.emplace_hint(selectedHiSpeedChanges.end(),
					                                    layered_tick_t{ l, tick });

		updateSelectionFlag();
	}

	void ScoreContext::deselectAll()
	{
		selectedNotes.clear();
		selectedHiSpeedChanges.clear();
		updateSelectionFlag();
	}

	void ScoreContext::deleteSelection()
	{
		if (!hasAnySelected())
			return;

		std::unordered_set<id_t> updatingHolds;
		NoteViewCollection deletingNotes = std::move(selectedNotes);
		selectedNotes.clear();
		for (auto&& [ID, _] : deletingNotes)
		{
			auto noteIt = score.notes.find(ID);
			if (noteIt == score.notes.end())
				// Note deleted or doesn't exist
				continue;
			Note& note = noteIt->second;
			if (note.isHold())
			{
				HoldNote& hold = score.holdNotes.at(note.holdID);
				if (hold.steps.size() == 2)
				{
					if (hold.steps.front() == ID)
						eraseNote(score.notes.at(hold.steps.back()), false);
					else
						eraseNote(score.notes.at(hold.steps.front()), false);
					score.holdNotes.erase(hold.ID);
				}
				else
				{
					// Erase the hold step
					auto it = std::find(hold.steps.begin(), hold.steps.end(), note.ID);
					assert(it != hold.steps.end() && "Note is not part of the hold?");
					if (!metadata.isExtendedScore)
					{
						// Make sure you only delete tick note in normal score
						if (*it == hold.steps.front())
							swapNoteProperties(note, score.notes.at(*std::next(it)));
						else if (*it == hold.steps.back())
							swapNoteProperties(note, score.notes.at(*std::prev(it)));
					}
					hold.steps.erase(it);
					// Erase the separator step
					auto stepIt = std::find_if(hold.separators.begin(), hold.separators.end(),
					                           [ID = note.ID](const HoldNoteStep& step)
					                           { return step.ID == ID; });
					if (stepIt != hold.separators.end())
						hold.separators.erase(stepIt);

					updatingHolds.emplace(hold.ID);
				}
			}
			eraseNote(note, false);
		}
		for (auto&& ID : updatingHolds)
		{
			auto holdIt = score.holdNotes.find(ID);
			if (holdIt == score.holdNotes.end())
				// Hold deleted after or doesn't exist
				continue;
			HoldNote& hold = holdIt->second;
			if (hold.separators.size() && hold.separators.front().ID == hold.steps.front())
			{
				static_cast<HoldNoteStep&>(hold) = hold.separators.front();
				hold.ID = ID;
				hold.separators.erase(hold.separators.begin());
			}
			hold.sortSteps(score.notes, !metadata.isExtendedScore);
		}
		for (auto& [layer, tick] : selectedHiSpeedChanges)
		{
			score.layers[layer].hiSpeedChanges.erase(tick);
		}

		deselectAll();
		hoveringNotes.clear();
		pushHistory("Delete notes", prev, score);
	}

	void ScoreContext::flipSelection()
	{
		if (hasAnyNoteSelected())
			return;

		Score prev = score;
		static_assert(int(FlickType::FlickTypeCount) == 7, "Make sure nothing broke here!");
		for (auto&& [_, pnote] : selectedNotes)
		{
			Note& note = *pnote;
			note.lane = ScoreEditorTimeline::NUM_LANES - note.lane - note.width + 1;

			switch (note.flick)
			{
			case FlickType::Left:
				note.flick = FlickType::Right;
				break;
			case FlickType::Right:
				note.flick = FlickType::Left;
				break;
			case FlickType::DownLeft:
				note.flick = FlickType::DownRight;
				break;
			case FlickType::DownRight:
				note.flick = FlickType::DownLeft;
				break;
			}
		}

		pushHistory("Flip notes", prev, score);
	}

	void ScoreContext::cutSelection()
	{
		copySelection();
		deleteSelection();
	}

	void ScoreContext::copySelection()
	{
		if (selectedNotes.empty() && selectedHiSpeedChanges.empty())
			return;

		int minTick = INT_MAX;
		if (!selectedNotes.empty())
		{
			minTick = score.notes
			              .at(*std::min_element(
			                  selectedNotes.begin(), selectedNotes.end(), [this](int id1, int id2)
			                  { return score.notes.at(id1).tick < score.notes.at(id2).tick; }))
			              .tick;
		}
		if (!selectedHiSpeedChanges.empty())
		{
			minTick = std::min(
			    minTick, score.hiSpeedChanges
			                 .at(*std::min_element(selectedHiSpeedChanges.begin(),
			                                       selectedHiSpeedChanges.end(),
			                                       [this](int id1, int id2) {
				                                       return score.hiSpeedChanges.at(id1).tick <
				                                              score.hiSpeedChanges.at(id2).tick;
			                                       }))
			                 .tick);
		}

		json data =
		    jsonIO::noteSelectionToJson(score, selectedNotes, selectedHiSpeedChanges, minTick);

		std::string clipboard{ clipboardSignature };
		clipboard.append(data.dump());

		ImGui::SetClipboardText(clipboard.c_str());
	}

	void ScoreContext::cancelPaste() { pasteData.pasting = false; }

	void ScoreContext::doPasteData(const json& data, bool flip)
	{
		int baseId = 0;
		pasteData.notes.clear();
		pasteData.damages.clear();
		pasteData.holds.clear();
		pasteData.hiSpeedChanges.clear();

		if (jsonIO::arrayHasData(data, "notes"))
		{
			for (const auto& entry : data["notes"])
			{
				Note note = jsonIO::jsonToNote(entry, NoteType::Tap);
				note.ID = baseId++;
				note.layer = selectedLayer;

				pasteData.notes[note.ID] = note;
			}
		}

		if (jsonIO::arrayHasData(data, "damages"))
		{
			for (const auto& entry : data["damages"])
			{
				Note note = jsonIO::jsonToNote(entry, NoteType::Damage);
				note.ID = baseId++;
				note.layer = selectedLayer;

				pasteData.damages[note.ID] = note;
			}
		}

		if (jsonIO::arrayHasData(data, "holds"))
		{
			for (const auto& entry : data["holds"])
			{
				if (!jsonIO::keyExists(entry, "start") || !jsonIO::keyExists(entry, "end"))
					continue;

				Note start = jsonIO::jsonToNote(entry["start"], NoteType::Hold);
				start.ID = baseId++;
				start.layer = selectedLayer;
				pasteData.notes[start.ID] = start;

				Note end = jsonIO::jsonToNote(entry["end"], NoteType::HoldEnd);
				end.ID = baseId++;
				end.parentID = start.ID;
				end.critical = start.critical || ((end.isFlick() || end.friction) && end.critical);
				end.layer = selectedLayer;
				pasteData.notes[end.ID] = end;

				std::string startEase =
				    jsonIO::tryGetValue<std::string>(entry["start"], "ease", "linear");

				HoldNote hold;
				hold.start = { start.ID, HoldStepType::Normal,
					           (EaseType)findArrayItem(startEase.c_str(), easeTypes,
					                                   arrayLength(easeTypes)) };
				hold.end = end.ID;
				for (int i = 0; i < arrayLength(fadeTypes); ++i)
				{
					if (entry["fade"] == fadeTypes[i])
					{
						hold.fadeType = (FadeType)i;
						break;
					}
				}
				for (int i = 0; i < arrayLength(guideColors); ++i)
				{
					if (entry["guide"] == guideColors[i])
					{
						hold.guideColor = (GuideColor)i;
						break;
					}
				}
				if (jsonIO::keyExists(entry, "steps"))
				{
					hold.steps.reserve(entry["steps"].size());
					for (const auto& step : entry["steps"])
					{
						Note mid = jsonIO::jsonToNote(step, NoteType::HoldMid);
						mid.critical = start.critical;
						mid.ID = baseId++;
						mid.parentID = start.ID;
						mid.layer = selectedLayer;
						pasteData.notes[mid.ID] = mid;

						std::string midType =
						    jsonIO::tryGetValue<std::string>(step, "type", "normal");
						std::string midEase =
						    jsonIO::tryGetValue<std::string>(step, "ease", "linear");
						int stepTypeIndex =
						    findArrayItem(midType.c_str(), stepTypes, arrayLength(stepTypes));
						int easeTypeIndex =
						    findArrayItem(midEase.c_str(), easeTypes, arrayLength(easeTypes));

						// Maintain compatibility with old step type names
						if (stepTypeIndex == -1)
						{
							stepTypeIndex = 0;
							if (midType == "invisible")
								stepTypeIndex = 1;
							if (midType == "ignored")
								stepTypeIndex = 2;
						}

						// Maintain compatibility with old ease type names
						if (easeTypeIndex == -1)
						{
							easeTypeIndex = 0;
							if (midEase == "in")
								easeTypeIndex = 1;
							if (midEase == "out")
								easeTypeIndex = 2;
						}

						hold.steps.push_back(
						    { mid.ID, (HoldStepType)stepTypeIndex, (EaseType)easeTypeIndex });
					}
				}

				std::string startType =
				    jsonIO::tryGetValue<std::string>(entry["start"], "type", "normal");
				std::string endType =
				    jsonIO::tryGetValue<std::string>(entry["end"], "type", "normal");

				if (startType == "guide" || endType == "guide")
				{
					hold.startType = hold.endType = HoldNoteType::Guide;
					start.friction = end.friction = false;
					end.flick = FlickType::None;
				}
				else
				{
					if (startType == "hidden")
					{
						hold.startType = HoldNoteType::Hidden;
						start.friction = false;
					}

					if (endType == "hidden")
					{
						hold.endType = HoldNoteType::Hidden;
						end.friction = false;
						end.flick = FlickType::None;
					}
				}

				hold.dummy = jsonIO::tryGetValue<bool>(entry, "dummy", false);

				pasteData.holds[hold.start.ID] = hold;
			}
		}

		int baseHiSpeedID = 0;

		if (jsonIO::arrayHasData(data, "hiSpeedChanges"))
		{
			for (const auto& entry : data["hiSpeedChanges"])
			{
				HiSpeedChange hs;
				hs.ID = baseHiSpeedID++;
				hs.tick = entry["tick"];
				hs.speed = entry["speed"];
				hs.skips = jsonIO::tryGetValue(entry, "skip", 0.0f);
				hs.ease = jsonIO::tryGetValue(entry, "ease", HiSpeedEaseType::None);
				hs.hideNotes = jsonIO::tryGetValue(entry, "hideNotes", false);

				pasteData.hiSpeedChanges[hs.ID] = hs;
			}
		}

		if (flip)
		{
			for (auto& [_, note] : pasteData.notes)
			{
				note.lane = MAX_LANE - note.lane - note.width + 1;

				if (note.flick == FlickType::Left)
					note.flick = FlickType::Right;
				else if (note.flick == FlickType::Right)
					note.flick = FlickType::Left;
			}
			for (auto& [_, note] : pasteData.damages)
			{
				note.lane = MAX_LANE - note.lane - note.width + 1;
			}
		}

		pasteData.pasting = !(pasteData.notes.empty() && pasteData.damages.empty() &&
		                      pasteData.holds.empty() && pasteData.hiSpeedChanges.empty());
		if (pasteData.pasting)
		{
			// find the lane in which the cursor is in the middle of pasted notes
			float extend = workingData.laneExtension;
			float left = MAX_LANE + extend;
			float right = MIN_LANE - extend;
			float leftmostLane = MAX_LANE + extend;
			float rightmostLane = MIN_LANE - extend;
			for (const auto& [_, note] : pasteData.notes)
			{
				leftmostLane = std::min((float)leftmostLane, note.lane);
				rightmostLane = std::max((float)rightmostLane, note.lane + note.width - 1);
				left = std::min((float)left, note.lane + note.width);
				right = std::max((float)right, note.lane);
			}

			pasteData.minLaneOffset = MIN_LANE - extend - leftmostLane;
			pasteData.maxLaneOffset = MAX_LANE + extend - rightmostLane;
			pasteData.midLane = (left + right) / 2;
		}
	}

	void ScoreContext::confirmPaste()
	{
		Score prev = score;

		std::unordered_map<int, int> noteIDMap;

		auto getNewID = [this, &noteIDMap](int oldID) -> int
		{
			if (noteIDMap.find(oldID) != noteIDMap.end())
				return noteIDMap[oldID];
			auto id = Note::getNextID();
			noteIDMap[oldID] = id;
			return id;
		};

		// update IDs and copy notes
		for (auto& [_, note] : pasteData.notes)
		{
			note.ID = getNewID(note.ID);
			if (note.parentID != -1)
				note.parentID = getNewID(note.parentID);

			note.lane += pasteData.offsetLane;
			note.tick += pasteData.offsetTicks;
			note.layer = selectedLayer;
			score.notes[note.ID] = note;
		}

		for (auto& [_, note] : pasteData.damages)
		{
			note.ID = getNewID(note.ID);
			if (note.parentID != -1)
				note.parentID = getNewID(note.parentID);

			note.lane += pasteData.offsetLane;
			note.tick += pasteData.offsetTicks;
			note.layer = selectedLayer;
			score.notes[note.ID] = note;
		}
		for (auto& [_, hold] : pasteData.holds)
		{
			hold.start.ID = getNewID(hold.start.ID);
			hold.end = getNewID(hold.end);
			for (auto& step : hold.steps)
				step.ID = getNewID(step.ID);

			score.holdNotes[hold.start.ID] = hold;
		}

		for (auto& [_, hsc] : pasteData.hiSpeedChanges)
		{
			hsc.ID = getNextHiSpeedID();
			hsc.layer = selectedLayer;
			hsc.tick += pasteData.offsetTicks;
			score.hiSpeedChanges[hsc.ID] = hsc;
		}

		// select newly pasted notes
		selectedNotes.clear();
		selectedHiSpeedChanges.clear();
		std::transform(pasteData.notes.begin(), pasteData.notes.end(),
		               std::inserter(selectedNotes, selectedNotes.end()),
		               [this](const auto& it) { return it.second.ID; });
		std::transform(pasteData.damages.begin(), pasteData.damages.end(),
		               std::inserter(selectedNotes, selectedNotes.end()),
		               [this](const auto& it) { return it.second.ID; });
		std::transform(pasteData.hiSpeedChanges.begin(), pasteData.hiSpeedChanges.end(),
		               std::inserter(selectedHiSpeedChanges, selectedHiSpeedChanges.end()),
		               [this](const auto& it) { return it.second.ID; });

		pasteData.pasting = false;
		pushHistory("Paste notes", prev, score);
	}

	void ScoreContext::paste(bool flip)
	{
		const char* clipboardDataPtr = ImGui::GetClipboardText();
		if (clipboardDataPtr == nullptr)
			return;

		std::string clipboardData(clipboardDataPtr);
		if (!startsWith(clipboardData, clipboardSignature))
			return;

		doPasteData(json::parse(clipboardData.substr(strlen(clipboardSignature))), flip);
	}

	void ScoreContext::duplicateSelection(bool flip)
	{
		copySelection();
		paste(flip);
	}

	{

		{


	void ScoreContext::shrinkSelection(tick_t spacing)
	{
		if ((selectedNotes.size() + selectedHiSpeedChanges.size()) < 2)
			return;

		std::unordered_set<id_t> updatingHold;
		std::vector<NoteOrderedCollection::node_type> updatingNodes;
		HiSpeedRefCollection updatingHispeed = std::move(selectedHiSpeedChanges);
		selectedHiSpeedChanges.clear();
		updatingNodes.reserve(selectedNotes.size());
		for (auto&& [ID, pnote] : selectedNotes)
		{
			auto&& [it, end] = notesOrderedView.equal_range(pnote->tick);
			it = std::find_if(it, end, [=](const NoteOrderedCollection::value_type& v)
			                  { return pnote == v.second; });
			if (it->second->isHold())
				updatingHold.emplace(it->second->holdID);
			updatingNodes.emplace_back(notesOrderedView.extract(it));
		}
		std::stable_sort(updatingNodes.begin(), updatingNodes.end(),
		                 [](const auto& n1, const auto& n2) { return n1.key() < n2.key(); });

		auto shrink = [&](auto ordFunc, auto noteIt, auto endNoteIt, auto hspdIt, auto endHspdIt)
		{
			tick_t baseTick;
			if (noteIt != endNoteIt && hspdIt != endHspdIt)
				baseTick = std::min(noteIt->key(), hspdIt->second, ordFunc);
			else if (noteIt != endNoteIt)
				baseTick = noteIt->key();
			else
				baseTick = hspdIt->second;
			for (tick_t tick = baseTick; noteIt != endNoteIt || hspdIt != endHspdIt;
			     tick += spacing)
			{
				if (noteIt != endNoteIt)
				{
					noteIt->key() = tick;
					noteIt->mapped()->tick = tick;
					notesOrderedView.insert(std::move(*noteIt));
					++noteIt;
				}
				if (hspdIt != endHspdIt &&
				    (tick == hspdIt->second ||
				     score.layers[hspdIt->first].hiSpeedChanges.count(tick) == 0))
				{
					auto& hiSpeedChanges = score.layers[hspdIt->first].hiSpeedChanges;
					auto node = hiSpeedChanges.extract(hspdIt->second);
					node.key() = tick;
					node.mapped().tick = tick;
					hiSpeedChanges.insert(std::move(node));
					selectedHiSpeedChanges.emplace(hspdIt->first, tick);
					++hspdIt;
				}
			}
		};

		if (spacing >= 0)
			shrink(std::less(), updatingNodes.begin(), updatingNodes.end(), updatingHispeed.begin(),
			       updatingHispeed.end());
		else
			shrink(std::greater(), updatingNodes.rbegin(), updatingNodes.rend(),
			       updatingHispeed.rbegin(), updatingHispeed.rend());

		for (const auto& holdID : updatingHold)
			score.holdNotes.at(holdID).sortSteps(score.notes, !metadata.isExtendedScore);

		pushHistory("Shrink notes");
	}

	void ScoreContext::compressSelection()
	{
		if (selectedNotes.size() < 2)
			return;
		Score prev = score;

		std::unordered_map<id_t, Layer> updatingLayers;
		std::unordered_set<id_t> updatingHold;
		std::vector<NoteOrderedCollection::node_type> updatingNodes;
		HiSpeedRefCollection updatingHispeed = std::move(selectedHiSpeedChanges);
		selectedHiSpeedChanges.clear();
		updatingNodes.reserve(selectedNotes.size());
		for (auto&& [ID, pnote] : selectedNotes)
		{
			auto&& [it, end] = notesOrderedView.equal_range(pnote->tick);
			it = std::find_if(it, end, [=](const NoteOrderedCollection::value_type& v)
			                  { return pnote == v.second; });
			if (it->second->isHold())
				updatingHold.emplace(it->second->holdID);
			updatingLayers.emplace(it->second->layer, score.layers[it->second->layer]);
			updatingNodes.emplace_back(notesOrderedView.extract(it));
		}
		std::stable_sort(updatingNodes.begin(), updatingNodes.end(),
		                 [](const auto& n1, const auto& n2) { return n1.key() < n2.key(); });

		auto noteIt = updatingNodes.begin(), endNoteIt = updatingNodes.end();
		auto hspdIt = updatingHispeed.begin(), endHspdIt = updatingHispeed.end();
		tick_t baseTick;
		if (noteIt != endNoteIt && hspdIt != endHspdIt)
			baseTick = std::min(noteIt->key(), hspdIt->second);
		else if (noteIt != endNoteIt)
			baseTick = noteIt->key();
		else
			baseTick = hspdIt->second;

		tick_t prevTick = baseTick;
		tick_t tick = baseTick;
		while (noteIt != endNoteIt || hspdIt != endHspdIt)
		{
			tick_t shrinkTick;
			int compare;
			if (noteIt != endNoteIt && hspdIt != endHspdIt)
			{
				compare = noteIt->key() == hspdIt->second  ? 0
				          : noteIt->key() < hspdIt->second ? -1
				                                           : 1;
				shrinkTick = std::min(noteIt->key(), hspdIt->second);
			}
			else
			{
				compare = noteIt != endNoteIt ? -1 : 1;
				shrinkTick = noteIt != endNoteIt ? noteIt->key() : hspdIt->second;
			}
			if (shrinkTick != prevTick)
				++tick;
			// Shrink notes
			if (compare <= 0)
			{
				noteIt->key() = tick;
				noteIt->mapped()->tick = tick;
				notesOrderedView.insert(std::move(*noteIt));
				++noteIt;
			}
			if (compare >= 0)
			{
				auto& hiSpeedChanges = score.layers[hspdIt->first].hiSpeedChanges;
				auto node = hiSpeedChanges.extract(hspdIt->second);
				node.key() = tick;
				node.mapped().tick = tick;
				hiSpeedChanges.erase(node.key());
				hiSpeedChanges.insert(std::move(node));
				selectedHiSpeedChanges.emplace(hspdIt->first, tick);
				++hspdIt;
			}
			// Add Hi-Speed
			if (shrinkTick != prevTick)
			{
				for (auto&& [layerID, layer] : updatingLayers)
				{
					float currSpeed = 1.0f;
					if (layer.hiSpeedChanges.size())
					{
						// Using lower bound here because we went the speed before the current tick
						auto it = layer.hiSpeedChanges.lower_bound(shrinkTick);
						if (it == layer.hiSpeedChanges.begin())
							currSpeed = it->second.speed;
						else
							currSpeed = std::prev(it)->second.speed;
					}

					auto& hiSpeedChanges = score.layers[layerID].hiSpeedChanges;
					float shrinkSpeed = (shrinkTick - prevTick) * currSpeed;
					auto it = hiSpeedChanges.upper_bound(tick - 1);
					if (it == hiSpeedChanges.begin() ||
					    (std::prev(it)->second.speed != shrinkSpeed &&
					     std::prev(it)->first != tick - 1))
						hiSpeedChanges.emplace(tick - 1, HiSpeed{ tick - 1, layerID, shrinkSpeed });
					else
						std::prev(it)->second.speed = shrinkSpeed;
					selectedHiSpeedChanges.emplace(layerID, tick - 1);
				}
			}
			prevTick = shrinkTick;
		}

		for (auto&& [layerID, layer] : updatingLayers)
		{
			float endSpeed = 1.0f;
			if (layer.hiSpeedChanges.size())
			{
				auto it = layer.hiSpeedChanges.upper_bound(prevTick);
				if (it == layer.hiSpeedChanges.begin())
					endSpeed = it->second.speed;
				else
					endSpeed = std::prev(it)->second.speed;
			}

			auto& hiSpeedChanges = score.layers[layerID].hiSpeedChanges;
			auto it = hiSpeedChanges.upper_bound(tick);
			if (it == hiSpeedChanges.begin() ||
			    (std::prev(it)->second.speed != endSpeed && std::prev(it)->first != tick))
				hiSpeedChanges.emplace(tick, HiSpeed{ tick, layerID, endSpeed });
			else
				std::prev(it)->second.speed = endSpeed;
			selectedHiSpeedChanges.emplace(layerID, tick);
		}

		for (const auto& holdID : updatingHold)
			score.holdNotes.at(holdID).sortSteps(score.notes, !metadata.isExtendedScore);

		pushHistory("Compress notes", prev, score);
	}

	void ScoreContext::connectHoldsInSelection()
	{
		if (selectedNotes.size() != 2)
			return;
		const Note& n1 = *selectedNotes.begin()->second;
		const Note& n2 = *(++selectedNotes.begin())->second;
		if (!n1.isHold() || !n2.isHold())
			return;
		if (n1.tick == n2.tick)
		{
			const HoldNote& h1 = score.holdNotes.at(n1.holdID);
			const HoldNote& h2 = score.holdNotes.at(n2.holdID);
			if (n2.ID == h2.steps.front() && n1.ID == h1.steps.back())
				connectHolds(n1.holdID, n2.holdID);
			else
				connectHolds(n2.holdID, n1.holdID);
		}
		else if (n1.tick < n2.tick)
			connectHolds(n1.holdID, n2.holdID);
		else
			connectHolds(n2.holdID, n1.holdID);
	}

	void ScoreContext::splitHoldInSelection()
	{
		if (selectedNotes.size() != 1)
			return;
		const Note& note = *selectedNotes.begin()->second;
		HoldNote& hold = score.holdNotes.at(note.holdID);
		splitHoldAt(hold, std::distance(hold.steps.begin(),
		                                std::find(hold.steps.begin(), hold.steps.end(), note.ID)));
	}

	void ScoreContext::repeatMidsInSelection()
	{

		int selectedTickNum = 0;
		for (const auto& noteId : selectedNotes)
		{
			auto& note = score.notes.at(noteId);
			if (note.hasEase())
			{
				selectedTickNum += 1;
			}
		}
		if (selectedTickNum < 3)
		{
			return;
		}

		Score prev = score;

		Note& note = score.notes[*selectedNotes.begin()];
		if (!(note.getType() == NoteType::HoldMid || note.getType() == NoteType::Hold))
			return;

		int holdIndex;

		if (note.getType() == NoteType::HoldMid)
		{
			holdIndex = note.parentID;
		}
		else
		{
			holdIndex = *selectedNotes.begin();
		}

		HoldNote& hold = score.holdNotes[holdIndex];

		std::vector<int> sortedSelection;

		for (const auto& noteId : selectedNotes)
		{
			auto& note = score.notes.at(noteId);
			if (note.hasEase())
			{
				sortedSelection.push_back(noteId);
			}
		}
		std::sort(sortedSelection.begin(), sortedSelection.end(),
		          [this](int a, int b) { return score.notes[a].tick < score.notes[b].tick; });

		Note& patternStart = score.notes.at(sortedSelection.front());
		Note& patternEnd = score.notes.at(sortedSelection.back());

		// TODO: Check this in ScoreEditorTimeline too (Otherwise it will become so unfriendly)
		/* if (patternStart.width != patternEnd.width) */
		/* 	return; */

		Note& holdStart = score.notes.at(hold.start.ID);
		Note& holdEnd = score.notes.at(hold.end);

		// score.notes.at(hold.start.ID).tick = 0;
		// score.notes.at(hold.end).flick = FlickType::Default;

		int patternHeight = patternEnd.tick - patternStart.tick;

		int iterations = std::floor((holdEnd.tick - holdStart.tick) / patternHeight);

		int startPos = findHoldStep(hold, patternStart.ID);
		int endPos = findHoldStep(hold, patternEnd.ID);

		if (startPos == -1)
		{
			hold.steps[endPos].ease = hold.start.ease;
		}
		else
		{
			hold.steps[endPos].ease = hold.steps[startPos].ease;
		}

		float minLane = MIN_LANE - workingData.laneExtension;
		float maxLane = MAX_LANE + workingData.laneExtension + 1;

		for (int j = 1; j < sortedSelection.size(); j++)
		{
			Note& currentRep = score.notes.at(sortedSelection[j]);
			int jPos = findHoldStep(hold, currentRep.ID);

			for (int i = 1; i < iterations; i++)
			{
				float lane = std::clamp(currentRep.lane + i * (patternEnd.lane - patternStart.lane),
				                        minLane, maxLane - currentRep.width);

				if (j == sortedSelection.size() - 1 && i == iterations - 1)
				{
					holdEnd.tick = currentRep.tick + patternHeight * i;
					holdEnd.lane = lane;
					holdEnd.width = currentRep.width;
					continue;
				}

				Note nextMid = Note(NoteType::HoldMid, currentRep.tick + patternHeight * i, lane,
				                    currentRep.width);

				nextMid.critical = patternStart.critical;

				nextMid.parentID = hold.start.ID;
				nextMid.layer = currentRep.layer;

				nextMid.ID = Note::getNextID();
				score.notes[nextMid.ID] = nextMid;

				HoldStepType type = jPos == -1 ? hold.steps[0].type : hold.steps[jPos].type;

				int temp = jPos;

				if (j == sortedSelection.size() - 1)
				{
					jPos = findHoldStep(hold, score.notes.at(sortedSelection[0]).ID);
				}

				EaseType ease = jPos == -1 ? hold.start.ease : hold.steps[jPos].ease;

				jPos = temp;

				hold.steps.push_back({ nextMid.ID, type, ease });
			}
		}

		sortHoldSteps(score, hold);

		pushHistory("Repeat hold mids", prev, score);
	}

	void ScoreContext::convertHoldToTraces(int quarterDivision, bool deleteHold, bool update)
	{
		if (!hasAnyNoteSelected())
			return;

		std::unordered_map<const HoldNoteStep*, id_t> updatingHoldSteps;
		for (auto [_, note] : selectedNotes)
		{
			if (!note->isHold())
				continue;

			const HoldNote& hold = score.holdNotes.at(note->holdID);
			const HoldNoteStep& step = hold.holdStepAt(*note, score.notes);
			if (hold.steps.front() != note->ID && (&step == &hold || step.ID != note->ID))
				continue;
			updatingHoldSteps.emplace(&step, hold.ID);
		}

		selectedNotes.clear();
		for (auto&& [pstep, holdID] : updatingHoldSteps)
		{
			HoldNote& hold = score.holdNotes.at(holdID);
			id_t startID, endID;
			if (&hold == pstep)
			{
				startID = hold.steps.front();
				endID = hold.separators.size() ? hold.separators.front().ID : hold.steps.back();
			}
			else
			{
				startID = pstep->ID;
				auto nextIt =
				    std::next(std::find_if(hold.separators.begin(), hold.separators.end(),
				                           [&](const HoldNoteStep& s) { return &s == pstep; }));
				endID = nextIt != hold.separators.end() ? nextIt->ID : hold.steps.back();
			}
			const Note& holdStart = score.notes.at(startID);
			const Note& holdEnd = score.notes.at(endID);
			auto compare = HoldNote::StepIdComparer(score.notes);
			auto nextJoint =
			    std::upper_bound(hold.joints.begin(), hold.joints.end(), holdStart, compare);
			// Find the connector head and tail for each trace note
			auto itJoint = std::prev(nextJoint);
			bool critical =
			    pstep->isCrit() || (pstep->isGuide() && pstep->guideColor == GuideColor::Yellow);
			tick_t tickPerDivision = TICKS_PER_QUARTER / quarterDivision;
			tick_t startTick = (holdStart.tick / tickPerDivision) * tickPerDivision;
			if (startTick < holdStart.tick)
				startTick += tickPerDivision;
			for (tick_t tick = startTick; tick <= holdEnd.tick; tick += tickPerDivision)
			{
				// Do not create trace notes if they will overlap with the hold start or end
				if (!deleteHold)
				{
					if (tick == holdStart.tick &&
					    !hasFlag(holdStart.flag, NoteFlag::Hidden | NoteFlag::Trace))
						continue;
					if (tick == holdEnd.tick &&
					    !hasFlag(holdEnd.flag, NoteFlag::Hidden | NoteFlag::Trace))
						continue;
				}

				const Note *head = &score.notes.at(*itJoint), *tail = &score.notes.at(*nextJoint);
				while (tail->tick < tick)
				{
					++itJoint;
					++nextJoint;
					head = tail;
					tail = &score.notes.at(*nextJoint);
				}

				float percent = unlerp(head->tick, tail->tick, tick);
				auto easeFunc = getEaseFunction(head->ease);
				float left = easeFunc(head->lane, tail->lane, percent);
				float right = easeFunc(head->lane + head->width, tail->lane + tail->width, percent);
				// Insert a trace note
				Note newNote;
				newNote.flag = setFlag(newNote.flag, NoteFlag::Critical, critical);
				newNote.flag = setFlag(newNote.flag, NoteFlag::Trace);
				newNote.flag = setFlag(newNote.flag, NoteFlag::Attached, metadata.isExtendedScore);
				newNote.layer = holdStart.layer;
				newNote.tick = tick;
				newNote.lane = metadata.isExtendedScore ? left : std::round(left);
				newNote.width = metadata.isExtendedScore ? right - left : std::round(right - left);

				Note* inserted = insertNote(newNote, !deleteHold ? holdID : -1, false);
				if (inserted)
					selectedNotes.emplace(inserted->ID, inserted);
			}

			hold.updateJoints(score.notes);
			hold.updateFading(score.notes);

			if (deleteHold)
			{
				if (hold.separators.empty())
					eraseHold(hold, false);
				else if (&hold == pstep)
				{
					size_t index = std::distance(
					    hold.steps.begin(), std::find(hold.steps.begin(), hold.steps.end(), endID));
					splitHoldAt(hold, index, false);
					eraseHold(hold, false);
				}
				else
				{
					size_t index = std::distance(
					    hold.steps.begin(), std::find(hold.steps.begin(), hold.steps.end(), endID));
					if (index > 0 && index < hold.steps.size() - 1)
						splitHoldAt(hold, index, false);
					index = std::distance(hold.steps.begin(),
					                      std::find(hold.steps.begin(), hold.steps.end(), startID));
					auto&& [_, delHoldID] = splitHoldAt(hold, index, false);
					eraseHold(score.holdNotes.at(delHoldID), false);
				}
			}
		}

		if (updatingHoldSteps.size())
		{
			updateSelectionFlag();
			pushHistory("Convert slides into traces");
		}
	}

	void ScoreContext::lerpHiSpeeds(int quarterDivision, EaseType ease)
	{
		if (selectedHiSpeedChanges.size() < 2)
			return;

		std::vector<layered_tick_t> insertedHispeed;
		const tick_t tickPerDivision = TICKS_PER_QUARTER / quarterDivision;
		const EaseFunction easeFunc = getEaseFunction(ease);
		constexpr tick_t MIN_TICK = 0;
		for (auto it = selectedHiSpeedChanges.begin(); it != selectedHiSpeedChanges.end();)
		{
			// Find the range of hispeeds in the same layer
			auto&& [layer, _] = *it;
			auto first = selectedHiSpeedChanges.lower_bound({ layer, MIN_TICK });
			auto last = selectedHiSpeedChanges.upper_bound({ layer, MAX_TICK });
			auto& hiSpeedChanges = score.layers[layer].hiSpeedChanges;

			for (auto next = std::next(first); next != last; first = next++)
			{
				const HiSpeed& hispeed = hiSpeedChanges.at(it->second);
				const HiSpeed& nxtHispeed = hiSpeedChanges.at(next->second);
				for (tick_t tick =
				         (hispeed.tick + tickPerDivision) / tickPerDivision * tickPerDivision;
				     tick < nxtHispeed.tick; tick += tickPerDivision)
				{
					float speed = easeFunc(hispeed.speed, nxtHispeed.speed,
					                       unlerp(hispeed.tick, nxtHispeed.tick, tick));
					auto insertIt = hiSpeedChanges.lower_bound(tick);
					if (insertIt == hiSpeedChanges.end() || insertIt->second.tick != tick)
						hiSpeedChanges.emplace_hint(insertIt, tick, HiSpeed{ tick, layer, speed });
					else
						insertIt->second.speed = speed;
					insertedHispeed.emplace_back(layer, tick);
				}
			}

			it = last;
		}

		selectedHiSpeedChanges.insert<std::vector<layered_tick_t>::iterator>(
		    insertedHispeed.begin(), insertedHispeed.end());

		if (insertedHispeed.size())
			pushHistory("Lerp hispeeds");
	}

	void ScoreContext::updateViews()
	{
		hoveringNotes.clear();
		notesOrderedView.clear();

		for (auto&& [ID, note] : score.notes)
		{
			notesOrderedView.emplace(note.tick, &note);
		}
	}

	void ScoreContext::undo()
	{
		if (history.hasUndo())
		{
			score = history.undo();
			clearSelection();

			//UI::setWindowTitle((workingData.filename.size()
			//                        ? File::getFilename(workingData.filename)
			//                        : windowUntitled) +
			//                   "*");
			upToDate = false;

			scoreStats.calculateStats(score);
		}
	}

	void ScoreContext::redo()
	{
		if (history.hasRedo())
		{
			score = history.redo();
			clearSelection();

			//UI::setWindowTitle((workingData.filename.size()
			//                        ? File::getFilename(workingData.filename)
			//                        : windowUntitled) +
			//                   "*");
			upToDate = false;

			scoreStats.calculateStats(score);
		}
	}

	void ScoreContext::convertHoldToGuide(GuideColor color)
	{
		if (!hasAnyNoteSelected())
			return;

		if (!metadata.isExtendedScore)
			color = color == GuideColor::Yellow ? color : GuideColor::Green;

		bool edit = false;
		std::unordered_map<HoldNoteStep*, id_t> updatingHoldSteps;
		for (auto [_, note] : selectedNotes)
		{
			if (!note->isHold())
				continue;

			HoldNote& hold = score.holdNotes.at(note->holdID);
			HoldNoteStep& step = hold.holdStepAt(*note, score.notes);
			if (hold.steps.front() != note->ID && (&step == &hold || step.ID != note->ID))
				continue;
			updatingHoldSteps.emplace(&step, hold.ID);
		}

		for (auto&& [pstep, holdID] : updatingHoldSteps)
		{
			edit |= !pstep->isGuide() || pstep->guideColor != color;
			pstep->flag = setFlag(pstep->flag, HoldNoteFlag::Guide);
			pstep->flag = setFlag(pstep->flag, HoldNoteFlag::Critical, false);
			pstep->guideColor = color;

			if (!metadata.isExtendedScore)
			{
				const HoldNote& hold = score.holdNotes.at(holdID);
				for (auto stepID : hold.steps)
				{
					Note& note = score.notes.at(stepID);
					note.flag = setFlag(note.flag, NoteFlag::Hidden);
				}
			}
		}

		if (edit)
			pushHistory("Convert hold to guide", prev, score);
	}

	void ScoreContext::convertGuideToHold(bool critical)
	void ScoreContext::convertGuideToHold()
	{
		if (!hasAnyNoteSelected())
			return;

		bool edit = false;
		std::unordered_map<HoldNoteStep*, id_t> updatingHoldSteps;
		for (auto [_, note] : selectedNotes)
		{
			if (!note->isHold())
				continue;

			HoldNote& hold = score.holdNotes.at(note->holdID);
			HoldNoteStep& step = hold.holdStepAt(*note, score.notes);
			if (hold.steps.front() != note->ID && (&step == &hold || step.ID != note->ID))
				continue;
			updatingHoldSteps.emplace(&step, hold.ID);
		}

		for (auto&& [pstep, holdID] : updatingHoldSteps)
		{
			edit |= pstep->isGuide() || pstep->isCrit() != critical;
			pstep->flag = setFlag(pstep->flag, HoldNoteFlag::Guide, false);
			pstep->flag = setFlag(pstep->flag, HoldNoteFlag::Critical, critical);

			if (!metadata.isExtendedScore)
			{
				const HoldNote& hold = score.holdNotes.at(holdID);
				for (auto stepID : hold.steps)
				{
					Note& note = score.notes.at(stepID);
					note.flag = setFlag(note.flag, NoteFlag::Critical, critical);
				}
			}
		}

		if (edit)
			pushHistory("Convert guide to hold", prev, score);
	}

	void ScoreContext::pushHistory(std::string description, const Score& prev, const Score& curr)
	{
		history.pushHistory(description, prev, curr);

		//UI::setWindowTitle((workingData.filename.size() ? File::getFilename(workingData.filename)
		//                                                : windowUntitled) +
		//                   "*");
		scoreStats.calculateStats(score);

		upToDate = false;
	}

	void ScoreContext::convertHoldToNone()
	{
		if (!hasAnyNoteSelected() || !metadata.isExtendedScore)
			return;

		std::unordered_map<const HoldNoteStep*, id_t> updatingHoldSteps;
		for (auto [_, note] : selectedNotes)
		{
			if (!note->isHold())
				continue;

			const HoldNote& hold = score.holdNotes.at(note->holdID);
			const HoldNoteStep& step = hold.holdStepAt(*note, score.notes);
			if (hold.steps.front() != note->ID && (&step == &hold || step.ID != note->ID))
				continue;
			updatingHoldSteps.emplace(&step, hold.ID);
		}

		selectedNotes.clear();
		for (auto&& [pstep, holdID] : updatingHoldSteps)
		{
			bool eraseStart = false, eraseEnd = false;
			HoldNote& hold = score.holdNotes.at(holdID);
			// Split the hold from it's current chain (if exist)
			if (hold.separators.size())
			{
				if (&hold == pstep)
				{
					size_t index = std::distance(hold.steps.begin(),
					                             std::find(hold.steps.begin(), hold.steps.end(),
					                                       hold.separators.front().ID));
					splitHoldAt(hold, index, false);
					eraseEnd = true;
				}
				else
				{
					auto nextIt =
					    std::next(std::find_if(hold.separators.begin(), hold.separators.end(),
					                           [&](const HoldNoteStep& s) { return &s == pstep; }));
					id_t endID = nextIt != hold.separators.end() ? nextIt->ID : hold.steps.back();

					size_t index = std::distance(
					    hold.steps.begin(), std::find(hold.steps.begin(), hold.steps.end(), endID));
					if (index > 0 && index < hold.steps.size() - 1)
					{
						splitHoldAt(hold, index, false);
						eraseEnd = true;
					}
					index =
					    std::distance(hold.steps.begin(),
					                  std::find(hold.steps.begin(), hold.steps.end(), pstep->ID));
					holdID = splitHoldAt(hold, index, false).second;
					eraseStart = true;
				}
			}

			HoldNote& erasingHold = holdID == hold.ID ? hold : score.holdNotes.at(holdID);
			if (eraseStart)
			{
				eraseNote(score.notes.at(erasingHold.steps.front()), false);
				erasingHold.steps.erase(erasingHold.steps.begin());
			}
			if (eraseEnd)
			{
				eraseNote(score.notes.at(erasingHold.steps.back()), false);
				erasingHold.steps.pop_back();
			}
			for (const auto& step : erasingHold.steps)
			{
				Note& note = score.notes.at(step);
				note.holdID = -1;
				note.flag = setFlag(note.flag, NoteFlag::LongNote, false);
				note.flag = setFlag(note.flag, NoteFlag::NonAttached, NoteFlag::Attached);
			}
			score.holdNotes.erase(erasingHold.ID);
		}

		if (updatingHoldSteps.size())
		{
			updateSelectionFlag();
			pushHistory("Convert slides into none");
		}
	}

	Note* ScoreContext::insertNote(const Note& note, id_t holdID, bool update)
	{
		if (!metadata.isExtendedScore)
		{
			switch (note.type)
			{
			case NoteType::Tap:
				if (holdID >= 0)
					holdID = -1;
				if (note.isHidden() || note.isDummy())
					return nullptr;
				break;
			case NoteType::Tick:
				if (holdID < 0 || note.isDummy())
					return nullptr;
				break;
			case NoteType::Damage:
			default:
				return nullptr;
			}
		}

		id_t nextID = nextNoteID++;
		Note& newNote = score.notes.emplace(nextID, note).first->second;
		newNote.ID = nextID;
		newNote.holdID = -1;
		newNote.width = !metadata.isExtendedScore ? std::floor(newNote.width) : newNote.width;
		newNote.width = std::clamp<float>(newNote.width, minNoteWidth(), maxNoteWidth());
		newNote.lane = !metadata.isExtendedScore ? std::floor(newNote.lane) : newNote.lane;
		newNote.lane = std::clamp<float>(newNote.lane, minLane(), maxLane(newNote.width));
		newNote.layer = selectedLayer;
		newNote.ease = newNote.ease >= maxEase() ? EaseType::Linear : newNote.ease;
		newNote.flick = newNote.flick >= maxFlick() ? FlickType::Default : newNote.flick;
		if (newNote.isHidden())
			newNote.flag = setFlag(newNote.flag, NoteFlag::Dummy, false);

		notesOrderedView.emplace(newNote.tick, &newNote);

		if (holdID >= 0)
		{
			HoldNote& hold = score.holdNotes.at(holdID);
			if (hold.isGuide() && !metadata.isExtendedScore)
				newNote.flag = setFlag(newNote.flag, NoteFlag::Hidden);
			hold.insertStep(newNote, score.notes, !metadata.isExtendedScore, update);
		}
		else
			newNote.flag = setFlag(newNote.flag, NoteFlag::Attached, false);

		if (update)
			pushHistory("Insert note");
		return &newNote;
	}

	void ScoreContext::eraseNote(Note& note, bool update)
	{
		auto it = score.notes.find(note.ID);
		if (it == score.notes.end())
			return;
		id_t noteID = note.ID;
		for (auto&& [it, end] = notesOrderedView.equal_range(note.tick); it != end; ++it)
		{
			if (it->second != &note)
				continue;
			notesOrderedView.erase(it);
			break;
		}
		score.notes.erase(it);
		if (selectedNotes.erase(noteID) && update)
			updateSelectionFlag();
		if (update)
			pushHistory("Erase note");
	}

	std::tuple<HoldNote&, Note&, Note&> ScoreContext::insertHold(const Note& start, const Note& end,
	                                                             const HoldNoteStep& step,
	                                                             bool update)
	{
		id_t nextIDH = nextHoldID++;

		id_t nextID = nextNoteID++;
		Note& holdStart = score.notes.emplace(nextID, start).first->second;
		holdStart.ID = nextID;
		holdStart.holdID = nextIDH;
		holdStart.width = !metadata.isExtendedScore ? std::floor(holdStart.width) : holdStart.width;
		holdStart.width = std::clamp<float>(holdStart.width, minNoteWidth(), maxNoteWidth());
		holdStart.lane = !metadata.isExtendedScore ? std::floor(holdStart.lane) : holdStart.lane;
		holdStart.lane = std::clamp<float>(holdStart.lane, minLane(), maxLane(holdStart.width));
		holdStart.layer = selectedLayer;
		holdStart.flag = setFlag(holdStart.flag, NoteFlag::LongNote);
		holdStart.flag = setFlag(holdStart.flag, NoteFlag::NonAttached, NoteFlag::Attached);
		holdStart.ease = holdStart.ease >= maxEase() ? EaseType::Linear : holdStart.ease;

		if (!metadata.isExtendedScore)
		{
			holdStart.flag = setFlag(holdStart.flag, NoteFlag::Dummy, false);
			holdStart.flick = FlickType::None;
			holdStart.type = NoteType::Tap;
			if (step.isGuide())
				holdStart.flag = setFlag(holdStart.flag, NoteFlag::Hidden);
		}

		nextID = nextNoteID++;
		Note& holdEnd = score.notes.emplace(nextID, end).first->second;
		holdEnd.ID = nextID;
		holdEnd.holdID = nextIDH;
		holdEnd.width = !metadata.isExtendedScore ? std::floor(holdEnd.width) : holdEnd.width;
		holdEnd.width = std::clamp<float>(holdEnd.width, minNoteWidth(), maxNoteWidth());
		holdEnd.lane = !metadata.isExtendedScore ? std::floor(holdEnd.lane) : holdEnd.lane;
		holdEnd.lane = std::clamp<float>(holdEnd.lane, minLane(), maxLane(holdEnd.width));
		holdEnd.layer = selectedLayer;
		holdEnd.flag = setFlag(holdEnd.flag, NoteFlag::LongNote);
		holdEnd.flag = setFlag(holdEnd.flag, NoteFlag::NonAttached, NoteFlag::Attached);
		holdEnd.ease = holdEnd.ease >= maxEase() ? EaseType::Linear : holdEnd.ease;

		if (!metadata.isExtendedScore)
		{
			holdEnd.flag = setFlag(holdEnd.flag, NoteFlag::Dummy, false);
			holdEnd.type = NoteType::Tap;
			if (step.isGuide())
				holdEnd.flag = setFlag(holdEnd.flag, NoteFlag::Hidden);
		}

		notesOrderedView.emplace(holdStart.tick, &holdStart);
		notesOrderedView.emplace(holdEnd.tick, &holdEnd);

		HoldNote& newHold = score.holdNotes[nextIDH];
		static_cast<HoldNoteStep&>(newHold) = step;
		newHold.ID = nextIDH;
		newHold.steps.clear();
		newHold.joints.clear();
		newHold.separators.clear();
		newHold.steps.push_back(holdStart.ID);
		newHold.steps.push_back(holdEnd.ID);
		newHold.joints.push_back(holdStart.ID);
		newHold.joints.push_back(holdEnd.ID);
		if (!metadata.isExtendedScore)
		{
			newHold.flag = setFlag(newHold.flag, HoldNoteFlag::Dummy, false);
			newHold.fadeType = FadeType::Classic;
			if (newHold.guideColor != GuideColor::Yellow)
				newHold.guideColor = GuideColor::Green;
		}

		if (update)
			newHold.updateFading(score.notes);

		if (update)
			pushHistory("Insert hold note");

		return { newHold, holdStart, holdEnd };
	}

	void ScoreContext::eraseHold(HoldNote& hold, bool update)
	{
		for (const auto& step : hold.steps)
			eraseNote(score.notes.at(step), false);
		score.holdNotes.erase(hold.ID);
		if (update)
		{
			updateSelectionFlag();
			pushHistory("Erase hold");
		}
	}

	id_t ScoreContext::connectHolds(id_t currHoldID, id_t nextHoldID, bool update)
	{
		HoldNote& currHold = score.holdNotes.at(currHoldID);
		HoldNote& nextHold = score.holdNotes.at(nextHoldID);
		const HoldNoteStep& currStep =
		    currHold.separators.size() ? currHold.separators.front() : currHold;

		Note& earlierStartNote = score.notes.at(
		    currHold.separators.size() ? currHold.separators.front().ID : currHold.steps.front());
		Note& earlierNote = score.notes.at(currHold.steps.back());
		Note& laterNote = score.notes.at(nextHold.steps.front());
		assert(earlierNote.tick <= laterNote.tick);

		bool startCrit = hasFlag(earlierStartNote.flag, NoteFlag::Critical);
		bool mergeNote = earlierNote.tick == laterNote.tick && earlierNote.lane == laterNote.lane &&
		                 earlierNote.width == laterNote.width;

		bool mergeHold = !metadata.isExtendedScore;
		if (currStep.isGuide() == false && nextHold.isGuide() == false)
			mergeHold |=
			    currStep.isCrit() == nextHold.isCrit() && currStep.isDummy() == nextHold.isDummy();
		else if (currStep.isGuide() == true && nextHold.isGuide() == true)
			mergeHold |= currStep.guideColor == nextHold.guideColor &&
			             currStep.fadeType == nextHold.fadeType;

		if (mergeNote)
		{
			eraseNote(earlierNote, false);
			currHold.steps.pop_back();
		}

		// Copy over latter hold steps
		for (auto& step : nextHold.steps)
		{
			currHold.steps.push_back(step);

			Note& note = score.notes.at(step);
			bool isLastStep = step == nextHold.steps.back();
			note.holdID = currHold.ID;

			if (!metadata.isExtendedScore)
			{
				if (!isLastStep)
					note.type = NoteType::Tick;
				bool isCrit = startCrit || isLastStep && (note.isFlick() || note.isTrace());
				note.flag = setFlag(note.flag, NoteFlag::Critical, isCrit);
				if (currStep.isGuide())
				{
					note.flag = setFlag(note.flag, NoteFlag::Hidden);
					note.flag = setFlag(note.flag, NoteFlag::Attached | NoteFlag::Dummy, false);
				}
			}
		}

		laterNote.flag = setFlag(laterNote.flag, NoteFlag::LongNote | NoteFlag::NonAttached, false);
		if (!mergeNote)
		{
			earlierNote.flag = setFlag(earlierNote.flag, NoteFlag::NonAttached, false);
			earlierNote.flag = setFlag(earlierNote.flag, NoteFlag::LongNote,
			                           currHold.separators.size() &&
			                               currHold.separators.back().ID == earlierNote.ID);
			if (!metadata.isExtendedScore)
			{
				earlierNote.type = NoteType::Tick;
				earlierNote.flag = setFlag(earlierNote.flag, NoteFlag::Critical, startCrit);
				if (currStep.isGuide())
				{
					earlierNote.flag = setFlag(earlierNote.flag, NoteFlag::Hidden);
					earlierNote.flag =
					    setFlag(earlierNote.flag, NoteFlag::Attached | NoteFlag::Dummy, false);
				}
			}
			if (HoldNote::StepComparer()(laterNote, earlierNote))
			{
				swapNotePosition(earlierNote, laterNote);
				swapNoteProperties(earlierNote, laterNote);
			}
		}
		else if (currHold.separators.size() &&
		         std::find(currHold.steps.begin(), currHold.steps.end(),
		                   currHold.separators.back().ID) == currHold.steps.end())
		{
			// The last step that merged was a separator, we need to delete the separator
			currHold.separators.pop_back();
		}
		if (!mergeHold)
		{
			nextHold.ID = laterNote.ID;
			currHold.separators.insert(currHold.separators.end(), nextHold);
			laterNote.flag = setFlag(laterNote.flag, NoteFlag::LongNote);
		}
		currHold.separators.insert(currHold.separators.end(), nextHold.separators.begin(),
		                           nextHold.separators.end());
		score.holdNotes.erase(nextHoldID);
		currHold.updateJoints(score.notes);
		currHold.updateFading(score.notes);

		if (update)
		{
			deselectAll();
			pushHistory("Connect holds");
		}

		return currHoldID;
	}

	std::pair<id_t, id_t> ScoreContext::splitHoldAt(HoldNote& hold, size_t index, bool update)
	{
		assert(index > 0 && index < hold.steps.size() - 1);
		Note& holdEnd = score.notes.at(hold.steps[index]);

		auto separatorNextIt = std::upper_bound(hold.separators.begin(), hold.separators.end(),
		                                        holdEnd, HoldNote::HoldStepComparer(score.notes));
		HoldNoteStep& holdStep =
		    separatorNextIt == hold.separators.begin() ? hold : *std::prev(separatorNextIt);
		HoldNote& newHold = score.holdNotes[nextHoldID];
		static_cast<HoldNoteStep&>(newHold) = holdStep;
		newHold.ID = nextHoldID++;
		newHold.steps.clear();
		newHold.joints.clear();
		newHold.separators.clear();

		if (!metadata.isExtendedScore)
		{
			newHold.flag = setFlag(newHold.flag, HoldNoteFlag::Dummy, false);
			newHold.fadeType = FadeType::Classic;
			if (newHold.guideColor != GuideColor::Yellow)
				newHold.guideColor = GuideColor::Green;

			holdEnd.flag = setFlag(holdEnd.flag, NoteFlag::Dummy, false);
			holdEnd.flick = FlickType::None;
			holdEnd.type = NoteType::Tap;
			holdEnd.flag = setFlag(holdEnd.flag, NoteFlag::Critical,
			                       hasFlag(newHold.flag, HoldNoteFlag::Critical));
			if (newHold.isGuide())
				holdEnd.flag = setFlag(holdEnd.flag, NoteFlag::Hidden);
		}
		holdEnd.flag = setFlag(holdEnd.flag, NoteFlag::LongNote);
		holdEnd.flag = setFlag(holdEnd.flag, NoteFlag::Attached, NoteFlag::NonAttached);
		Note& newHoldStart = score.notes[nextNoteID] = holdEnd;
		newHoldStart.ID = nextNoteID++;
		newHoldStart.holdID = newHold.ID;
		newHold.steps.push_back(newHoldStart.ID);

		notesOrderedView.emplace(newHoldStart.tick, &newHoldStart);

		for (size_t i = index + 1; i < hold.steps.size(); ++i)
		{
			Note& stepNote = score.notes.at(hold.steps[i]);
			stepNote.holdID = newHold.ID;
			newHold.steps.push_back(stepNote.ID);
		}
		hold.steps.erase(hold.steps.begin() + index + 1, hold.steps.end());
		std::copy(separatorNextIt, hold.separators.end(), std::back_inserter(newHold.separators));
		bool erasePrev = separatorNextIt != hold.separators.begin() && holdStep.ID == holdEnd.ID;
		hold.separators.erase(separatorNextIt - erasePrev, hold.separators.end());

		hold.updateJoints(score.notes);
		hold.updateFading(score.notes);
		newHold.updateJoints(score.notes);
		newHold.updateFading(score.notes);

		if (update)
		{
			selectedNotes.clear();
			selectedHiSpeedChanges.clear();
			selectNote(holdEnd);
			selectNote(newHoldStart);
			pushHistory("Split hold");
		}

		return { hold.ID, newHold.ID };
	}

	void ScoreContext::insertTempoChange(const Tempo& tempo)
	{
		auto&& [tempoIt, inserted] = score.tempoChanges.try_emplace(tempo.tick, tempo);
		if (!inserted)
		{
			auto&& [_, existedTempo] = *tempoIt;
			inserted = existedTempo.quarterPerMinute != tempo.quarterPerMinute;
			existedTempo.quarterPerMinute = tempo.quarterPerMinute;
		}
		if (inserted)
			pushHistory("Insert BPM change");
	}

	void ScoreContext::insertTimeSignature(const TimeSignature& timeSig)
	{
		auto&& [timeSigIt, inserted] = score.timeSignatures.try_emplace(timeSig.measure, timeSig);
		if (!inserted)
		{
			auto&& [_, existedTS] = *timeSigIt;
			inserted |= existedTS.numerator != timeSig.numerator;
			inserted |= existedTS.denominator != timeSig.denominator;
			existedTS.numerator = timeSig.numerator;
			existedTS.denominator = timeSig.denominator;
		}
		if (inserted)
			pushHistory("Insert time signature");
	}

	HiSpeed& ScoreContext::insertHispeedChange(const HiSpeed& hispeed, bool update)
	{
		HiSpeed newHispeed = hispeed;
		newHispeed.layer = selectedLayer;
		if (!metadata.isExtendedScore)
		{
			newHispeed.skips = 0;
			newHispeed.ease = HiSpeedEaseType::None;
			newHispeed.hideNotes = false;
		}
		auto& hispeedChanges = score.layers[selectedLayer].hiSpeedChanges;
		auto&& [hispeedIt, inserted] = hispeedChanges.try_emplace(hispeed.tick, newHispeed);
		if (!inserted)
		{
			auto&& [_, existedHspd] = *hispeedIt;
			inserted |= existedHspd.speed != newHispeed.speed;
			inserted |= existedHspd.skips != newHispeed.skips;
			inserted |= existedHspd.ease != newHispeed.ease;
			inserted |= existedHspd.hideNotes != newHispeed.hideNotes;
			existedHspd = newHispeed;
		}
		if (inserted && update)
			pushHistory("Insert hi-speed changes");
		return hispeedIt->second;
	}

	Waypoint& ScoreContext::insertWaypoint(tick_t tick, const std::string& name)
	{
		id_t nextID = score.waypoints.size() ? score.waypoints.rbegin()->first + 1 : 0;
		auto&& [_, waypoint] = *score.waypoints.emplace_hint(score.waypoints.end(), nextID,
		                                                     Waypoint{ nextID, tick, name });
		waypointOrderedView.emplace(tick, &waypoint);
		pushHistory("Create new waypoint");
		return waypoint;
	}

	void ScoreContext::eraseWaypoint(id_t waypointID)
	{
		auto it = score.waypoints.find(waypointID);
		if (it == score.waypoints.end())
			return;
		for (auto [orderedIt, orderedEnd] = waypointOrderedView.equal_range(it->second.tick);
		     orderedIt != orderedEnd; ++orderedIt)
		{
			auto&& [_, waypoint] = *orderedIt;
			if (waypoint->ID == waypointID)
			{
				waypointOrderedView.erase(orderedIt);
				score.waypoints.erase(it);
				pushHistory("Remove waypint");
				return;
			}
		}
	}

	void ScoreContext::insertSkill(tick_t tick)
	{
		auto&& [_, inserted] = score.skills.insert(tick);
		if (inserted)
			pushHistory("Insert skill");
	}

	bool ScoreContext::isLayerVisible(id_t layer) const
	{
		return showAllLayers || !score.layers[layer].hidden;
	}

	bool ScoreContext::isLayerInteractive(id_t layer) const
	{
		return showAllLayers || layer == selectedLayer;
	}

	bool ScoreContext::isLayerSelected(id_t layer) const { return layer == selectedLayer; }
}
