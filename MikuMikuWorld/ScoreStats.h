#pragma once

namespace MikuMikuWorld
{
	struct Score;

	struct ScoreStats
	{
		int taps, flicks, holds, guides, ticks, traces, damages, dummies, hispeeds, total, combo;

		ScoreStats();

		void calculateStats(const Score& score);
		void calculateCombo(const Score& score);
		void reset();
	};
}