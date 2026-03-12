#include "Tempo.h"
#include "Constants.h"
#include "Score.h"
#include <algorithm>

namespace MikuMikuWorld
{
	float quatersPerMeasure(const TimeSignature& t) { return 4.0f * t.numerator / t.denominator; }
	beat_t beatsPerMeasure(const TimeSignature& t) { return t.numerator; }
	tick_t ticksPerBeat(const TimeSignature& t, tick_t quarterTicks)
	{
		return quarterTicks * quatersPerMeasure(t) / beatsPerMeasure(t);
	}

	qnote_t ticksToQuarters(tick_t ticks, tick_t quarterTicks)
	{
		return static_cast<qnote_t>(ticks) / quarterTicks;
	}

	tick_t quartersToTicks(qnote_t quarters, tick_t quarterTicks)
	{
		static_assert(sizeof(long) == sizeof(tick_t));
		return std::lround(quarters * quarterTicks);
	}

	secs_t quartersToSecs(qnote_t quarters, float minuteQuarters)
	{
		return quarters * 60.f / minuteQuarters;
	}

	qnote_t secsToQuarters(secs_t secs, float minuteQuarters)
	{
		return secs * minuteQuarters / 60.f;
	}

	secs_t accumulateDuration(tick_t tick, const TempoCollection& tempos, tick_t quarterTicks)
	{
		if (tempos.empty())
			return 0;
		secs_t total = 0;

		auto last = tempos.upper_bound(tick);
		if (last == tempos.begin()) // probably negative tick
			++last;
		for (auto pv = tempos.begin(), it = std::next(pv); it != last; pv = it, ++it)
		{
			auto&& [prevTick, prevTempo] = *pv;
			auto&& [currTick, currTempo] = *it;
			total += quartersToSecs(ticksToQuarters(currTick - prevTick, quarterTicks),
			                        prevTempo.quarterPerMinute);
		}

		auto&& [lastTick, lastTempo] = *std::prev(last);
		total += quartersToSecs(ticksToQuarters(tick - lastTick, quarterTicks),
		                        lastTempo.quarterPerMinute);

		return total;
	}

	tick_t accumulateTicks(secs_t secs, const TempoCollection& tempos, tick_t quarterTicks)
	{
		if (tempos.empty())
			return 0;
		tick_t total = 0;
		secs_t accSecs = 0;

		auto pv = tempos.begin();
		for (auto it = std::next(pv); it != tempos.end(); pv = it, ++it)
		{
			auto&& [prevTick, prevTempo] = *pv;
			auto&& [currTick, currTempo] = *it;

			tick_t ticks = currTick - prevTick;
			float seconds =
			    quartersToSecs(ticksToQuarters(ticks, quarterTicks), prevTempo.quarterPerMinute);

			if (accSecs + seconds >= secs)
				break;
			total += ticks;
			accSecs += seconds;
		}

		total += quartersToTicks(secsToQuarters(secs - accSecs, pv->second.quarterPerMinute),
		                         quarterTicks);

		return total;
	}

	measure_t accumulateMeasures(tick_t tick, const TimeSignatureCollection& ts,
	                             tick_t quarterTicks)
	{
		if (ts.empty())
			return 0;
		const qnote_t endQuarts = ticksToQuarters(tick, quarterTicks);
		qnote_t accQuarts = 0;
		measure_t total = 0;

		auto pv = ts.begin();
		for (auto it = std::next(pv); it != ts.end(); pv = it, ++it)
		{
			auto&& [prevMeasure, prevTimeSig] = *pv;
			auto&& [currMeasure, currTimeSig] = *it;

			measure_t measures = currMeasure - prevMeasure;
			qnote_t quarters = measures * quatersPerMeasure(prevTimeSig);

			if ((accQuarts + quarters) >= endQuarts)
				break;
			total += measures;
			accQuarts += quarters;
		}

		total += (endQuarts - accQuarts) / quatersPerMeasure(pv->second);

		return total;
	}

	tick_t accumulateTicks(measure_t measure, const TimeSignatureCollection& ts,
	                       tick_t quarterTicks)
	{
		if (ts.empty())
			return 0;
		tick_t total = 0;

		auto last = ts.upper_bound(measure);
		for (auto pv = ts.begin(), it = std::next(pv); it != last; pv = it, ++it)
		{
			auto&& [prevMeasure, prevTimeSig] = *pv;
			auto&& [currMeasure, currTimeSig] = *it;

			measure_t measures = currMeasure - prevMeasure;
			qnote_t quarters = measures * quatersPerMeasure(prevTimeSig);

			total += quartersToTicks(quarters, quarterTicks);
		}

		auto&& [prevMeasure, prevTimeSig] = *std::prev(last);
		total +=
		    quartersToTicks((measure - prevMeasure) * quatersPerMeasure(prevTimeSig), quarterTicks);

		return total;
	}

	beat_t accumulateBeats(measure_t measure, const TimeSignatureCollection& ts)
	{
		if (ts.empty())
			return 0;
		float total = 0;

		auto last = ts.upper_bound(measure);
		for (auto pv = ts.begin(), it = std::next(pv); it != last; pv = it, ++it)
		{
			auto&& [prevMeasure, prevTimeSig] = *pv;
			auto&& [currMeasure, currTimeSig] = *it;

			measure_t measures = currMeasure - prevMeasure;
			total += measures * beatsPerMeasure(prevTimeSig);
		}

		auto&& [prevMeasure, prevTimeSig] = *std::prev(last);
		total += (measure - prevMeasure) * beatsPerMeasure(prevTimeSig);

		return total;
	}

	TimeSignature& getTimeSignAt(Score& score, measure_t measure)
	{
		return std::prev(score.timeSignatures.upper_bound(measure))->second;
	}

	Tempo& getTempoAt(Score& score, tick_t tick)
	{
		return std::prev(score.tempoChanges.upper_bound(tick))->second;
	}

	HiSpeed* getHiSpeedAt(Score& score, tick_t tick, id_t layer)
	{
		auto& currentHispeedChanges = score.layers[layer].hiSpeedChanges;
		if (currentHispeedChanges.empty())
			return nullptr;
		return &std::prev(currentHispeedChanges.upper_bound(tick))->second;
	}

	bool insertTempoChange(TempoCollection& collection, const Tempo& tempo)
	{
		return collection.try_emplace(tempo.tick, tempo).second;
	}

	bool isSame(const HiSpeed& a, const HiSpeed& b)
	{
		return a.speed == b.speed && a.ease == b.ease && a.skips == b.skips &&
		       a.hideNotes == b.hideNotes;
	}
}
