/*
    Mostly taken from https://github.com/samyuu/PeepoDrumKit/blob/master/src/audio/audio_waveform.h
    with heavy modifications to fit the current audio engine
*/

#pragma once
#include "AudioManager.h"
#include <cstdint>
#include <vector>

namespace Audio
{
	class WaveformMip
	{
	  public:
		size_t sampleCount{};
		double secondsPerSample{};
		double samplesPerSecond{};
		std::vector<int16_t> absoluteSamples;

		double getDuration() const;
		int16_t getSampleAtIndex(size_t index) const;
		int16_t linearSampleAtSeconds(double seconds) const;
		float averageNormalizedSampleInTimeRange(double startTime, double endTime) const;
	};

	class WaveformMipChain
	{
		// Derived from the timeline min and max zoom
		// = log2(1 / ((unit / zoom) / avg(timelineScreenHeight)))
		static constexpr uint32_t maxSampleRate{ 1u << 14 };
		static constexpr uint32_t minSampleRate{ 1u << 5 };
		static constexpr uint32_t maxSamples{ 1u << 30 }; // ~2GB of sample data
	  public:
		std::vector<WaveformMip> mips;
		double durationInSeconds{};

		bool isEmpty() const { return mips.empty(); }

		void clear()
		{
			durationInSeconds = 0;
			mips.clear();
		}

		int getUsedMipCount() const { return mips.size(); }

		const WaveformMip& findClosestMip(double secondsPerPixel) const;
		float getAmplitudeAt(const WaveformMip& mip, double seconds, double secondsPerPixel) const;
		void generateMipChainsFromSampleBuffer(const SoundBuffer& audioData, uint32_t channelIndex);
	};
}
