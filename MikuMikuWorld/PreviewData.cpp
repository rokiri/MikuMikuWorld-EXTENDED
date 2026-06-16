#include <queue>
#include <stdexcept>
#include "PreviewData.h"
#include "PreviewEngine.h"
#include "Tempo.h"
#include "ApplicationConfiguration.h"
#include "Constants.h"
#include "ResourceManager.h"
#include "ScoreContext.h"
#include <algorithm>
#include "ScoreStats.h"

namespace MikuMikuWorld::Engine
{
	double LayerHiSpeedCache::getStm(int tick) const
	{
		if (nodes.empty()) return 0.0;
		auto it = std::upper_bound(nodes.begin(), nodes.end(), tick, [](int t, const HiSpeedCacheNode& node) {
			return t < node.tick;
		});
		if (it != nodes.begin()) --it;
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

	static void addHoldNote(DrawData& drawData, const HoldNote& holdNote, Score const &score);

	void DrawData::calculateDrawData(Score const &score)
	{
		this->clear();
		try
		{
			hsCache.clear();
			hsCache.resize(score.layers.size());
			for (int layer = 0; layer < score.layers.size(); ++layer)
			{
				std::vector<HiSpeedChange> hsList;
				for (const auto& [id, hs] : score.hiSpeedChanges)
					if (hs.layer == layer) hsList.push_back(hs);
				
				std::sort(hsList.begin(), hsList.end(), [](const HiSpeedChange& a, const HiSpeedChange& b) {
					return a.tick < b.tick;
				});

				// テンポとHSが変化する全てのTick（境界点）を収集
				std::vector<int> boundaries;
				boundaries.push_back(0);
				for (const auto& tempo : score.tempoChanges) boundaries.push_back(tempo.tick);
				for (const auto& hs : hsList) boundaries.push_back(hs.tick);
				std::sort(boundaries.begin(), boundaries.end());
				boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

				// 各境界点における「正確な視覚的時間」と「次の境界までの速度」を計算して保存
				for (int tick : boundaries)
				{
					double stm = accumulateScaledDuration(tick, TICKS_PER_BEAT, score.tempoChanges, score.hiSpeedChanges, layer);
					
					double bpm = 120.0;
					for (auto it = score.tempoChanges.rbegin(); it != score.tempoChanges.rend(); ++it)
						if (it->tick <= tick) { bpm = it->bpm; break; }

					double speed = 1.0;
					for (auto it = hsList.rbegin(); it != hsList.rend(); ++it)
						if (it->tick <= tick) { speed = it->speed; break; }

					double speedPerTick = (60.0 / bpm) * speed / TICKS_PER_BEAT;
					hsCache[layer].nodes.push_back({ tick, stm, speedPerTick });
				}
			}

			this->noteSpeed = config.pvNoteSpeed;

			{
				constexpr float SCORE_TEAM_POWER = 300000.0f;
				constexpr float SCORE_RATING = 26.0f;

				auto lerpScore = [](float value, float start, float end, float startPos,
				                    float endPos) -> float
				{
					if (end <= start)
						return endPos;
					return ((value - start) / (end - start)) * (endPos - startPos) + startPos;
				};

				auto scoreRankAndBar = [&](float score) -> std::pair<char, float>
				{
					const float rankBorder = 1200000.0f + (SCORE_RATING - 5.0f) * 4100.0f;
					const float rankS = 1040000.0f + (SCORE_RATING - 5.0f) * 5200.0f;
					const float rankA = 840000.0f + (SCORE_RATING - 5.0f) * 4200.0f;
					const float rankB = 400000.0f + (SCORE_RATING - 5.0f) * 2000.0f;
					const float rankC = 20000.0f + (SCORE_RATING - 5.0f) * 100.0f;

					constexpr float rankBorderPos = 1650.0f / 1650.0f;
					constexpr float rankSPos = 1478.0f / 1650.0f;
					constexpr float rankAPos = 1234.0f / 1650.0f;
					constexpr float rankBPos = 990.0f / 1650.0f;
					constexpr float rankCPos = 746.0f / 1650.0f;

					auto clamp01 = [](float v) { return std::max(0.0f, std::min(1.0f, v)); };

					if (score >= rankBorder)
						return { 's', rankBorderPos };
					if (score >= rankS)
						return { 's', clamp01(lerpScore(score, rankS, rankBorder, rankSPos,
							                            rankBorderPos)) };
					if (score >= rankA)
						return { 'a', clamp01(lerpScore(score, rankA, rankS, rankAPos, rankSPos)) };
					if (score >= rankB)
						return { 'b', clamp01(lerpScore(score, rankB, rankA, rankBPos, rankAPos)) };
					if (score >= rankC)
						return { 'c', clamp01(lerpScore(score, rankC, rankB, rankCPos, rankBPos)) };
					return { 'd', clamp01((score / std::max(rankC, 1.0f)) * rankCPos) };
				};

				const std::vector<ComboEvent> comboEvents = calculateComboEvents(score);
				comboTimes.reserve(comboEvents.size());
				hudScores.reserve(comboEvents.size());
				hudRanks.reserve(comboEvents.size());
				hudScoreBars.reserve(comboEvents.size());
				hudJudgeTimes.reserve(comboEvents.size());

				float weightedCount = 0.0f;
				for (const ComboEvent& e : comboEvents)
					weightedCount += std::max(0.0f, e.weight);
				weightedCount = std::max(weightedCount, 1.0f);

				const float levelFactor = (SCORE_RATING - 5.0f) * 0.005f + 1.0f;
				int combo = 0;
				float comboFactor = 1.0f;
				float hudScore = 0.0f;

				for (const ComboEvent& e : comboEvents)
				{
					const float timeSec =
					    accumulateDuration(e.tick, TICKS_PER_BEAT, score.tempoChanges);
					const float weight = std::max(0.0f, e.weight);
					combo++;
					if (combo % 100 == 1 && combo > 1)
						comboFactor = std::min(comboFactor + 0.01f, 1.1f);

					hudScore += (SCORE_TEAM_POWER / weightedCount) * 4.0f * weight * levelFactor *
					            comboFactor;
					const auto [rank, scoreBar] = scoreRankAndBar(hudScore);

					comboTimes.push_back(timeSec);
					hudScores.push_back(std::max(0, static_cast<int>(std::lround(hudScore))));
					hudRanks.push_back(rank);
					hudScoreBars.push_back(scoreBar);
					if (e.showJudge)
						hudJudgeTimes.push_back(timeSec);
				}
			}

			// =========================================================================
			// 【変更】同じTickに存在するノーツの <中心X座標, レイヤー> のリストを保持する
			// =========================================================================
			std::map<int, std::vector<std::pair<float, int>>> simBuilder;
			
			for (const auto& [id, note] : score.notes)
			{
				if (note.layer >= 0 && note.layer < score.layers.size() && score.layers[note.layer].hidden)
					continue;

				maxTicks = std::max(note.tick, maxTicks);
				NoteType type = note.getType();
				
				if (type == NoteType::HoldMid
					|| (type == NoteType::Hold && score.holdNotes.at(id).startType != HoldNoteType::Normal)
					|| (type == NoteType::HoldEnd && score.holdNotes.at(note.parentID).endType != HoldNoteType::Normal))
					continue;
				if (type == NoteType::HoldMid)
					continue;
					
				auto visual_tm = getNoteVisualTime(note, score, noteSpeed);
				drawingNotes.push_back(DrawingNote{note.ID, visual_tm, type, note.dummy, note.layer});

				// 同時押し線構築のためにリストに追加する
				float center = getNoteCenter(note);
				simBuilder[note.tick].push_back({center, note.layer});
			}

			// =========================================================================
			// 【変更】実機仕様：左右のノーツの情報を抽出して DrawingLine を構築する
			// =========================================================================
			for (const auto& [line_tick, notesAtTick] : simBuilder)
			{
				// 同じTickに2つ以上のノーツがある場合
				if (notesAtTick.size() > 1)
				{
					// そのTickにあるノーツの中で、一番左と一番右のノーツを探す
					auto minmax = std::minmax_element(notesAtTick.begin(), notesAtTick.end(), [](const auto& a, const auto& b) {
						return a.first < b.first;
					});
					
					// 左右の座標が違う（完全に重なっていない）場合のみ同時押し線を生成
					if (minmax.first->first != minmax.second->first)
					{
						drawingLines.push_back(DrawingLine{
							line_tick,           // leftTick
							minmax.first->first, // leftLane
							minmax.first->second,// leftLayer

							line_tick,            // rightTick
							minmax.second->first, // rightLane
							minmax.second->second // rightLayer
						});
					}
				}
			}

			for (const auto& [id, holdNote] : score.holdNotes)
			{
				addHoldNote(*this, holdNote, score);
			}
		}
		catch(const std::out_of_range& ex)
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

		comboTimes.clear();
		hudScores.clear();
		hudRanks.clear();
		hudScoreBars.clear();
		hudJudgeTimes.clear();

		maxTicks = 1;
	}

	void addHoldNote(DrawData &drawData, const HoldNote &holdNote, Score const &score)
	{
		const Note& startNote = score.notes.at(holdNote.start.ID);
		const Note& endNote = score.notes.at(holdNote.end);

		if (startNote.layer >= 0 && startNote.layer < score.layers.size() && score.layers[startNote.layer].hidden)
			return;

		float noteDuration = getNoteDuration(drawData.noteSpeed);
		float activeTime = accumulateDuration(startNote.tick, TICKS_PER_BEAT, score.tempoChanges);
		float startTime = activeTime;
		DrawingHoldStep head = {
			startNote.tick,
			accumulateScaledDuration(startNote.tick, TICKS_PER_BEAT, score.tempoChanges, score.hiSpeedChanges, startNote.layer),
			Engine::laneToLeft(startNote.lane),
			Engine::laneToLeft(startNote.lane) + startNote.width,
			holdNote.start.ease
		};
		
		for (ptrdiff_t headIdx = -1, tailIdx = 0, stepSz = holdNote.steps.size(); headIdx < stepSz; ++tailIdx)
		{
			if (tailIdx < stepSz && holdNote.steps[tailIdx].type == HoldStepType::Skip)
				continue;
			HoldStep tailStep = tailIdx == stepSz ? HoldStep{ holdNote.end, HoldStepType::Hidden } : holdNote.steps[tailIdx];
			const Note& tailNote = score.notes.at(tailStep.ID);
			auto easeFunction = getEaseFunction(head.ease);
			DrawingHoldStep tail = {
				tailNote.tick,
				accumulateScaledDuration(tailNote.tick, TICKS_PER_BEAT, score.tempoChanges, score.hiSpeedChanges, startNote.layer),
				Engine::laneToLeft(tailNote.lane),
				Engine::laneToLeft(tailNote.lane) + tailNote.width,
				tailStep.ease
			};
			float endTime = accumulateDuration(tailNote.tick, TICKS_PER_BEAT, score.tempoChanges);
			
			drawData.drawingHoldSegments.push_back(DrawingHoldSegment {
				holdNote.end, 
				head.ease,
				holdNote.isGuide(),
				holdNote.guideColor,
				holdNote.dummy,
				startNote.layer,
				tailIdx,
				head.time, tail.time,
				head.left, head.right,
				tail.left, tail.right,
				startTime, endTime,
				activeTime,
				head.tick, tail.tick
			});
			startTime = endTime;
			
			while ((headIdx + 1) < tailIdx)
			{
				const HoldStep& skipStep = holdNote.steps[headIdx + 1];
				assert(skipStep.type == HoldStepType::Skip);
				const Note& skipNote = score.notes.at(skipStep.ID);
				if (skipNote.tick > tail.tick)
					break;
				double tickTime = accumulateScaledDuration(skipNote.tick, TICKS_PER_BEAT, score.tempoChanges, score.hiSpeedChanges, startNote.layer);
				double tick_t = unlerpD(head.tick, tail.tick, skipNote.tick);
				float skipLeft = easeFunction(head.left, tail.left, tick_t);
				float skipRight = easeFunction(head.right, tail.right, tick_t);
				
				drawData.drawingHoldTicks.push_back(DrawingHoldTick{
					skipStep.ID,
					skipLeft + (skipRight - skipLeft) / 2,
					Range{tickTime - noteDuration, tickTime},
					holdNote.dummy,
					startNote.layer
				});
				++headIdx;
			}
			if (tailStep.type != HoldStepType::Hidden)
			{
				double tickTime = accumulateScaledDuration(tailNote.tick, TICKS_PER_BEAT, score.tempoChanges, score.hiSpeedChanges, startNote.layer);
				drawData.drawingHoldTicks.push_back(DrawingHoldTick{
					tailNote.ID,
					getNoteCenter(tailNote),
					{tickTime - noteDuration, tickTime},
					holdNote.dummy,
					startNote.layer
				});
			}
			head = tail;
			++headIdx;
		}
	}
}
