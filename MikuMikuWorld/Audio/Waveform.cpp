#include "Waveform.h"
#include "../Math.h"
#include <limits>

namespace Audio
{
	constexpr int16_t int16_t_max = std::numeric_limits<int16_t>::max();

	double WaveformMip::getDuration() const { return absoluteSamples.size() / samplesPerSecond; }

	int16_t WaveformMip::getSampleAtIndex(size_t index) const
	{
		if (index >= absoluteSamples.size())
			return 0;

		return absoluteSamples[index];
	}

	int16_t WaveformMip::linearSampleAtSeconds(double seconds) const
	{
		double sampleIndex,
		    sampleIndexFraction = std::modf(seconds * samplesPerSecond, &sampleIndex);

		const size_t sampleIndexLo = std::max(0ull, static_cast<size_t>(sampleIndex));
		const size_t sampleIndexHi = sampleIndexLo + 1;

		const float normalizedSampleLo =
		    getSampleAtIndex(sampleIndexLo) / static_cast<float>(int16_t_max);
		const float normalizedSampleHi =
		    getSampleAtIndex(sampleIndexHi) / static_cast<float>(int16_t_max);
		const float normalizedSample =
		    MikuMikuWorld::lerp(normalizedSampleLo, normalizedSampleHi, sampleIndexFraction);

		return static_cast<int16_t>(normalizedSample * static_cast<float>(int16_t_max));
	}

	float WaveformMip::averageNormalizedSampleInTimeRange(double startTime, double endTime) const
	{
		if (secondsPerSample <= 0)
		{
			assert(false);
			return 0.0f;
		}

		int32_t sampleSum = 0, sampleCount = 0;
		for (double t = startTime; t < endTime; t += secondsPerSample)
		{
			sampleSum += linearSampleAtSeconds(t);
			sampleCount++;
		}

		const int32_t sampleAverage = sampleSum / std::max(sampleCount, 1);
		return sampleAverage / static_cast<float>(int16_t_max);
	}

	const WaveformMip& WaveformMipChain::findClosestMip(double secondsPerPixel) const
	{
		auto it = std::lower_bound(mips.begin(), mips.end(), secondsPerPixel,
		                           [](const WaveformMip& mip, double secondsPerPixel)
		                           { return mip.secondsPerSample <= secondsPerPixel; });
		return it == mips.begin() ? *it : *(--it);
	}

	float WaveformMipChain::getAmplitudeAt(const WaveformMip& mip, double seconds,
	                                       double secondsPerPixel) const
	{
		return mip.averageNormalizedSampleInTimeRange(seconds, seconds + secondsPerPixel);
	}

	void WaveformMipChain::generateMipChainsFromSampleBuffer(const SoundBuffer& audioData,
	                                                         uint32_t channelIndex)
	{
		clear();
		if (!audioData.isValid())
			return;

		durationInSeconds =
		    static_cast<float>(audioData.frameCount) / static_cast<float>(audioData.sampleRate);

		struct BlockData
		{
			int32_t sum;
			int32_t max;
		};
		std::vector<BlockData> blockSample;
		size_t baseSampleCount = audioData.frameCount;
		double baseSampleRate = audioData.sampleRate;
		int samplingSize = 1;
		while (baseSampleRate > maxSampleRate || baseSampleCount > maxSamples)
		{
			baseSampleCount /= 2;
			baseSampleRate /= 2;
			samplingSize *= 2;

			if (baseSampleRate < minSampleRate)
				break;
		}

		// Calculate the absolute samples from the original samples
		channelIndex %= audioData.channelCount;
		blockSample.resize(baseSampleCount, { 0, 0 });
		for (size_t frameIndex = 0; frameIndex < baseSampleCount; frameIndex++)
		{
			for (size_t sample = 0; sample < samplingSize; ++sample)
			{
				size_t sampleIndex = frameIndex * samplingSize + sample;
				int32_t audioSample = std::abs(
				    audioData.samples[sampleIndex * audioData.channelCount + channelIndex]);
				blockSample[frameIndex].sum += audioSample;
				blockSample[frameIndex].max =
				    std::max(audioSample, blockSample[frameIndex].max);
			}
		}

		while (baseSampleRate >= minSampleRate)
		{
			// Calculate the average of max and mean for each block
			// This ensure the relative shape of the waveform when zooming
			// While keeping the peaks in the waveform not flatten out
			WaveformMip& newMip = mips.emplace_back();
			newMip.sampleCount = baseSampleCount;
			newMip.samplesPerSecond = baseSampleRate;
			newMip.secondsPerSample = 1 / baseSampleRate;
			newMip.absoluteSamples.resize(baseSampleCount);
			auto sampleIt = newMip.absoluteSamples.begin();
			for (auto&& [sum, max] : blockSample)
				*(sampleIt++) = static_cast<int16_t>((sum / samplingSize + max) / 2);

			// Down sample the buffer for the next mip
			samplingSize *= 2;
			size_t newSampleCount = baseSampleCount / 2 + baseSampleCount % 2;
			double newSampleRate = baseSampleRate / 2;
			if (newSampleRate < minSampleRate)
				break;
			for (size_t i = 0; i < newSampleCount; ++i)
			{
				size_t AIndex = i * 2, BIndex = i * 2 + 1;

				int32_t sampleA = blockSample[AIndex].sum;
				int32_t sampleB = BIndex >= baseSampleCount ? 0.f : blockSample[BIndex].sum;
				blockSample[i].sum = sampleA + sampleB;

				sampleA = blockSample[AIndex].max;
				sampleB = BIndex >= baseSampleCount ? 0.f : blockSample[BIndex].max;
				blockSample[i].max = std::max(sampleA, sampleB);
			}
			blockSample.resize(newSampleCount);
			baseSampleCount = newSampleCount;
			baseSampleRate = newSampleRate;
		}
	}

}
