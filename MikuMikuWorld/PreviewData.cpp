#include <stdexcept>
#include "PreviewData.h"
#include "PreviewEngine.h"
#include "Tempo.h"
#include "ApplicationConfiguration.h"
#include "Constants.h"
#include <algorithm>

namespace MikuMikuWorld::Engine
{
	double LayerHiSpeedCache::getStm(int tick) const
	{
		if (nodes.empty())
			return 0.0;
		auto it =
		    std::upper_bound(nodes.begin(), nodes.end(), tick,
		                     [](int t, const HiSpeedCacheNode& node) { return t < node.tick; });
		if (it != nodes.begin())
			--it;
		return it->stm + (tick - it->tick) * it->speedPerTick;
	}

	struct DrawingHoldStep
	{
		int tick;
		double time;
		float left;
		float right;
		EaseType ease;
	};

	static void addHoldNote(DrawData& drawData, const HoldNote& holdNote, Score const& score);

	void DrawData::calculateDrawData(Score const& score)
	{
		this->clear();
		try
		{
			hsCache.clear();
			hsCache.resize(score.layers.size());
			for (int layer = 0; layer < (int)score.layers.size(); ++layer)
			{
				const HiSpeedCollection& hsList = score.layers[layer].hiSpeedChanges;

				std::vector<int> boundaries;
				boundaries.push_back(0);
				for (const auto& [tick, tempo] : score.tempoChanges)
					boundaries.push_back(tempo.tick);
				for (const auto& [tick, hs] : hsList)
					boundaries.push_back(tick);
				std::sort(boundaries.begin(), boundaries.end());
				boundaries.erase(std::unique(boundaries.begin(), boundaries.end()),
				                 boundaries.end());

				for (int tick : boundaries)
				{
					double stm = accumulateScaledDuration(tick, score);

					double bpm = 120.0;
					for (auto it = score.tempoChanges.rbegin(); it != score.tempoChanges.rend();
					     ++it)
						if (it->second.tick <= tick)
						{
							bpm = it->second.quarterPerMinute;
							break;
						}

					double speed = 1.0;
					for (auto it = hsList.rbegin(); it != hsList.rend(); ++it)
						if (it->first <= tick)
						{
							speed = it->second.speed;
							break;
						}

					double speedPerTick = (60.0 / bpm) * speed / TICKS_PER_QUARTER;
					hsCache[layer].nodes.push_back({ tick, stm, speedPerTick });
				}
			}

			this->noteSpeed = getConfig().pvNoteSpeed;

			std::map<int, std::vector<std::pair<float, int>>> simBuilder;

			for (const auto& [id, note] : score.notes)
			{
				if (note.layer >= 0 && note.layer < (int)score.layers.size() &&
				    score.layers[note.layer].hidden)
					continue;

				maxTicks = std::max(note.tick, maxTicks);

				if (note.isHidden() || note.type == NoteType::Damage)
					continue;

				if (note.type == NoteType::Tick && note.isAttached())
					continue;

				auto visual_tm = getNoteVisualTime(note, score, noteSpeed);
				drawingNotes.push_back(
				    DrawingNote{ note.ID, visual_tm, note.type, note.isDummy(), note.layer });

				if (note.canSimLine())
				{
					float center = getNoteCenter(note);
					simBuilder[note.tick].push_back({ center, note.layer });
				}
			}

			for (const auto& [line_tick, notesAtTick] : simBuilder)
			{
				if (notesAtTick.size() > 1)
				{
					auto minmax = std::minmax_element(notesAtTick.begin(), notesAtTick.end(),
					                                  [](const auto& a, const auto& b)
					                                  { return a.first < b.first; });

					if (minmax.first->first != minmax.second->first)
					{
						drawingLines.push_back(
						    DrawingLine{ line_tick, minmax.first->first, minmax.first->second,
						                 line_tick, minmax.second->first, minmax.second->second });
					}
				}
			}

			for (const auto& [id, holdNote] : score.holdNotes)
				addHoldNote(*this, holdNote, score);
		}
		catch (const std::out_of_range&)
		{
			this->clear();
		}
	}

	void DrawData::clear()
	{
		drawingLines.clear();
		drawingNotes.clear();
		drawingHoldTicks.clear();
		drawingHoldSegments.clear();
		maxTicks = 1;
	}

	void addHoldNote(DrawData& drawData, const HoldNote& holdNote, Score const& score)
	{
		if (holdNote.steps.size() < 2)
			return;

		const Note& startNote = score.notes.at(holdNote.steps.front());
		const Note& endNote = score.notes.at(holdNote.steps.back());

		if (startNote.layer >= 0 && startNote.layer < (int)score.layers.size() &&
		    score.layers[startNote.layer].hidden)
			return;

		bool isGuide = !holdNote.separators.empty() && holdNote.separators.front().isGuide();
		GuideColor guideColor = !holdNote.separators.empty()
		                            ? holdNote.separators.front().guideColor
		                            : GuideColor::Green;
		bool isDummy = !holdNote.separators.empty() && holdNote.separators.front().isDummy();

		float noteDuration = getNoteDuration(drawData.noteSpeed);
		float activeTime = (float)accumulateDuration(startNote.tick, score.tempoChanges);
		float startTime = activeTime;

		DrawingHoldStep head = { startNote.tick, accumulateScaledDuration(startNote.tick, score),
			                     laneToLeft(startNote.lane),
			                     laneToLeft(startNote.lane) + startNote.width, startNote.ease };

		for (int tailIdx = 1; tailIdx < (int)holdNote.steps.size(); ++tailIdx)
		{
			const Note& tailNote = score.notes.at(holdNote.steps[tailIdx]);

			if (tailNote.isAttached() && tailIdx < (int)holdNote.steps.size() - 1)
			{
				double tickTime = accumulateScaledDuration(tailNote.tick, score);
				drawData.drawingHoldTicks.push_back(DrawingHoldTick{
				    tailNote.ID, getNoteCenter(tailNote),
				    Range{ tickTime - noteDuration, tickTime }, isDummy, startNote.layer });
				continue;
			}

			DrawingHoldStep tail = { tailNote.tick, accumulateScaledDuration(tailNote.tick, score),
				                     laneToLeft(tailNote.lane),
				                     laneToLeft(tailNote.lane) + tailNote.width, tailNote.ease };

			float endTime = (float)accumulateDuration(tailNote.tick, score.tempoChanges);

			drawData.drawingHoldSegments.push_back(DrawingHoldSegment{
			    endNote.ID, head.ease, isGuide, guideColor, isDummy, startNote.layer,
			    (ptrdiff_t)tailIdx, head.time, tail.time, head.left, head.right, tail.left,
			    tail.right, startTime, endTime, activeTime, head.tick, tail.tick });

			startTime = endTime;

			if (!tailNote.isHidden() && tailIdx < (int)holdNote.steps.size() - 1)
			{
				double tickTime = accumulateScaledDuration(tailNote.tick, score);
				drawData.drawingHoldTicks.push_back(DrawingHoldTick{
				    tailNote.ID, getNoteCenter(tailNote),
				    Range{ tickTime - noteDuration, tickTime }, isDummy, startNote.layer });
			}

			head = tail;
		}
	}
}