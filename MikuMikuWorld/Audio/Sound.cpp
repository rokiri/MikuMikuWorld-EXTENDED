#include "../IO.h"
#include "../PlatformIO.h"
#include "../File.h"
#include "../Math.h"
#include "../Stopwatch.h"

#include "Sound.h"
#include <algorithm>

namespace Audio
{
	namespace mmw = MikuMikuWorld;

	void SoundBuffer::initialize(const std::string& name, ma_uint32 sampleRate,
	                             ma_uint32 channelCount, ma_uint64 frameCount, int16_t* samples)
	{
		this->name = name;
		this->sampleFormat = ma_format_s16;
		this->channelCount = channelCount;
		this->frameCount = frameCount;
		this->sampleRate = sampleRate;
		this->samples = std::unique_ptr<int16_t[]>(samples);
		this->effectiveSampleRate = sampleRate;

		ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
		    this->sampleFormat, channelCount, frameCount, this->samples.get(), nullptr);
		bufferConfig.sampleRate = sampleRate;
		ma_audio_buffer_init(&bufferConfig, &buffer);
	}

	void SoundBuffer::dispose()
	{
		name.clear();
		ma_audio_buffer_uninit(&buffer);
		samples.reset();

		sampleFormat = ma_format_unknown;
		sampleRate = 0;
		channelCount = 0;
		frameCount = 0;
		effectiveSampleRate = 0;
	}

	bool isSupportedFileFormat(std::string_view fileExtension)
	{
		for (auto&& format : { ".mp3", ".wav", ".flac", ".ogg" })
			if (format == fileExtension)
				return true;
		return false;
	}

	ma_uint64 SoundPool::getDurationInFrames() const
	{
		ma_uint64 length{};
		ma_data_source_get_length_in_pcm_frames(pool[0].source.pDataSource, &length);

		return length;
	}

	float SoundPool::getDurationInSeconds() const
	{
		float length{};
		ma_data_source_get_length_in_seconds(pool[0].source.pDataSource, &length);

		return length;
	}

	void SoundPool::setLoopTime(ma_uint64 startFrames, ma_uint64 endFrames)
	{
		for (auto& instance : pool)
			ma_data_source_set_loop_point_in_pcm_frames(instance.source.pDataSource, startFrames,
			                                            endFrames);
	}

	bool SoundPool::isLooping() const { return hasFlag(flags, SoundFlags::LOOP); }

	void SoundPool::setVolume(float volume)
	{
		for (auto& instance : pool)
			ma_sound_set_volume(&instance.source, volume);
	}

	float SoundPool::getVolume() const { return ma_sound_get_volume(&pool[0].source); }

	void SoundPool::initialize(const std::string& path, ma_engine* engine, ma_sound_group* group,
	                           SoundFlags flags)
	{
		for (int i = 0; i < pool.size(); i++)
		{
			ma_result result =
			    ma_sound_init_from_file_w(engine, IO::utf8ToWide(path).c_str(),
			                              maSoundFlagsDecodeAsync, group, NULL, &pool[i].source);
			if (hasFlag(flags, SoundFlags::LOOP))
				ma_sound_set_looping(&pool[i].source, true);
		}

		this->flags = flags;
		currentIndex = 0;
	}

	void SoundPool::initialize(ma_sound* source, ma_engine* engine, ma_sound_group* group,
	                           SoundFlags flags)
	{
		for (int i = 0; i < pool.size(); i++)
		{
			ma_result result =
			    ma_sound_init_copy(engine, source, maSoundFlagsDecodeAsync, group, &pool[i].source);
			if (hasFlag(flags, SoundFlags::LOOP))
				ma_sound_set_looping(&pool[i].source, true);
		}

		this->flags = flags;
		currentIndex = 0;
	}

	void SoundPool::initialize(SoundBuffer& sound, ma_engine* engine, ma_sound_group* group,
	                           SoundFlags flags)
	{
		for (int i = 0; i < pool.size(); i++)
		{
			ma_result result = ma_sound_init_from_data_source(
			    engine, &sound.buffer, maSoundFlagsDecodeAsync, group, &pool[i].source);
			if (hasFlag(flags, SoundFlags::LOOP))
				ma_sound_set_looping(&pool[i].source, true);
		}

		this->flags = flags;
		currentIndex = 0;
	}

	void SoundPool::dispose()
	{
		for (auto& instance : pool)
			ma_sound_uninit(&instance.source);
	}

	void SoundPool::play(float start, float end)
	{
		SoundInstance& instance = pool[currentIndex];

		instance.seek(0);
		ma_sound_set_start_time_in_milliseconds(&instance.source, start * 1000);

		if (end >= 0)
			ma_sound_set_stop_time_in_milliseconds(&instance.source, end * 1000);

		instance.play();
		instance.absoluteStart = start;
		instance.absoluteEnd = end;

		if (!hasFlag(flags, SoundFlags::EXTENDABLE))
			currentIndex = ++currentIndex % pool.size();
	}

	void SoundPool::stopAll()
	{
		for (auto& instance : pool)
		{
			instance.stop();
			instance.absoluteStart = 0;
			instance.absoluteEnd = 0;
		}
	}

	bool SoundPool::isPlaying(const SoundInstance& soundInstance) const
	{
		return soundInstance.isPlaying();
	}

	bool SoundPool::isAnyPlaying() const
	{
		return std::any_of(pool.begin(), pool.end(),
		                   [this](const SoundInstance& a) { return isPlaying(a); });
	}
}
