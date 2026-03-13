#define MINIAUDIO_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define DR_WAV_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#include "AudioManager.h"

#include "../Application.h"
#include "../IO.h"
#include "../UI.h"
#include "../Note.h"

#include <execution>

#undef STB_VORBIS_HEADER_ONLY

namespace Audio
{
	namespace mmw = MikuMikuWorld;

	void AudioManager::initializeAudioEngine()
	{
		std::string err = "";
		ma_result result = MA_SUCCESS;

		try
		{
			result = ma_engine_init(nullptr, &engine);
			if (result != MA_SUCCESS)
			{
				err = "FATAL: Failed to start audio engine. Aborting.\n";
				throw(result);
			}

			result = ma_sound_group_init(&engine, maSoundFlagsDefault, nullptr, &musicGroup);
			if (result != MA_SUCCESS)
			{
				err = "FATAL: Failed to initialize music sound group. Aborting.\n";
				throw(result);
			}

			result = ma_sound_group_init(&engine, maSoundFlagsDefault, nullptr, &soundEffectsGroup);
			if (result != MA_SUCCESS)
			{
				err = "FATAL: Failed to initialize sound effects sound group. Aborting.\n";
				throw(result);
			}
		}
		catch (ma_result)
		{
			err.append(ma_result_description(result));
			IO::messageBox(APP_NAME, err, IO::MessageBoxButtons::Ok, IO::MessageBoxIcon::Error);

			exit(result);
		}
	}

	void AudioManager::startEngine() { ma_engine_start(&engine); }

	void AudioManager::stopEngine() { ma_engine_stop(&engine); }

	bool AudioManager::isEngineStarted() const
	{
		return ma_device_get_state(engine.pDevice) == ma_device_state_started;
	}

	void AudioManager::loadSoundEffects()
	{
		constexpr size_t soundEffectsCount = std::size(mmw::SE_NAMES);

		baseSounds.reserve(soundEffectsCount * soundEffectsProfileCount);

		for (size_t profile = 0; profile < soundEffectsProfileCount; profile++)
		{
			auto path = mmw::Application::getInstance().getResourcePath(
			    "sound", IO::formatString("%02d\\", profile + 1));
			for (size_t i = 0; i < soundEffectsCount; ++i)
			{
				SoundInstance& debugSound = baseSounds.emplace_back();
				auto filename = (path / mmw::SE_NAMES[i]).wstring();
				filename.append(L".mp3");
				ma_sound_init_from_file_w(&engine, filename.c_str(), maSoundFlagsDecodeAsync,
				                          &soundEffectsGroup, nullptr, &debugSound.source);
			}
		}

		initializeCurrentSoundProfile();
	}

	void AudioManager::initializeMusic(ma_sound* music, ma_audio_buffer* buffer)
	{
		ma_sound_init_from_data_source(&engine, buffer, MA_SOUND_FLAG_NO_SPATIALIZATION,
		                               &musicGroup, music);
	}

	std::shared_ptr<SoundEffectProfile> AudioManager::getCurrentSoundEffectsProfile()
	{
		return sounds;
	}

	void AudioManager::uninitializeAudioEngine()
	{
		uninitializeSoundProfile();
		ma_engine_uninit(&engine);
	}

	float AudioManager::getMasterVolume() const { return masterVolume; }

	void AudioManager::setMasterVolume(float volume)
	{
		masterVolume = volume;
		ma_engine_set_volume(&engine, volume);
	}

	float AudioManager::getMusicVolume() const { return musicVolume; }

	void AudioManager::setMusicVolume(float volume)
	{
		musicVolume = volume;
		ma_sound_group_set_volume(&musicGroup, volume);
	}

	float AudioManager::getSoundEffectsVolume() const { return soundEffectsVolume; }

	void AudioManager::setSoundEffectsVolume(float volume)
	{
		soundEffectsVolume = volume;
		ma_sound_group_set_volume(&soundEffectsGroup, volume);
	}

	uint32_t AudioManager::getDeviceChannelCount() const
	{
		return engine.pDevice->playback.channels;
	}

	float AudioManager::getDeviceLatency() const
	{
		return engine.pDevice->playback.internalPeriodSizeInFrames /
		       static_cast<float>(engine.pDevice->playback.internalSampleRate);
	}

	uint32_t AudioManager::getDeviceSampleRate() const
	{
		return engine.pDevice->playback.internalSampleRate;
	}

	float AudioManager::getAudioEngineAbsoluteTime() const
	{
		return static_cast<float>(ma_engine_get_time_in_milliseconds(&engine)) / 1000.0f;
	}

	void Audio::AudioManager::setAudioEngineAbsoluteTime(float time)
	{
		ma_engine_set_time_in_milliseconds(&engine, time * 1000.0f);
	}

	uint32_t AudioManager::getAudioEngineSampleRate() const { return engine.sampleRate; }

	int AudioManager::getSoundEffectsProfileIndex() const { return soundEffectsProfileIndex; }

	void AudioManager::setSoundEffectsProfileIndex(int index)
	{
		if (soundEffectsProfileIndex == index)
			return;
		soundEffectsProfileIndex = index;
		uninitializeSoundProfile();
		initializeCurrentSoundProfile();
	}

	void AudioManager::initializeCurrentSoundProfile()
	{
		const size_t soundEffectsCount = std::size(mmw::SE_NAMES);
		sounds = std::make_shared<SoundEffectProfile>();
		sounds->pool.reserve(soundEffectsCount);
		constexpr float soundEffectsVolumes[] = {
			0.75f, 0.75f, 0.90f, 0.80f, 0.70f, 0.75f, 0.80f, 0.92f, 0.82f, 0.70f, 0.25f
		};
		static_assert(std::size(soundEffectsVolumes) == soundEffectsCount);

		for (size_t i = 0; i < soundEffectsCount; ++i)
		{
			bool isConnectSound =
			    mmw::SE_NAMES[i] == mmw::SE_CONNECT || mmw::SE_NAMES[i] == mmw::SE_CRITICAL_CONNECT;
			SoundFlags flags =
			    isConnectSound ? SoundFlags::LOOP | SoundFlags::EXTENDABLE : SoundFlags::NONE;

			auto&& [it, _] = sounds->pool.emplace(mmw::SE_NAMES[i], std::make_unique<SoundPool>());
			auto& pool = it->second;
			pool->initialize(&baseSounds[i + soundEffectsProfileIndex * soundEffectsCount].source,
			                 &engine, &soundEffectsGroup, flags);
			pool->setVolume(soundEffectsVolumes[i]);

			if (isConnectSound)
			{
				// Adjust hold SE loop times for gapless playback
				ma_uint64 holdDuration = pool->getDurationInFrames();
				pool->setLoopTime(3000, holdDuration - 3000);
			}
		}
	}

	void AudioManager::uninitializeSoundProfile()
	{
		for (auto&& [_, pool] : sounds->pool)
		{
			pool->stopAll();
			pool->dispose();
		}
		sounds->pool.clear();
		sounds.reset();
	}

	AudioContext::AudioContext(AudioManager& manager) : manager(manager), music()
	{
		soundEffectsProfileIndex = manager.getSoundEffectsProfileIndex();
		soundEffects = manager.getCurrentSoundEffectsProfile();
	}

	void AudioContext::playMusic(float currentTime)
	{
		seekMusic(currentTime - musicOffset);

		// Starting past the music end
		if (isMusicAtEnd())
			return;

		float time = musicOffset + manager.getAudioEngineAbsoluteTime() - currentTime;
		ma_sound_set_start_time_in_milliseconds(&music, std::max(0.0f, time * 1000));
		ma_sound_start(&music);
	}

	void AudioContext::stopMusic() { ma_sound_stop(&music); }

	void AudioContext::seekMusic(float musicTime)
	{
		ma_uint64 seekFrame = std::max(musicTime, 0.f) * musicBuffer.sampleRate;
		ma_sound_seek_to_pcm_frame(&music, seekFrame);

		ma_uint64 length{};
		ma_result lengthResult = ma_sound_get_length_in_pcm_frames(&music, &length);
		if (lengthResult != MA_SUCCESS)
			return;

		if (seekFrame > length)
		{
			// Seeking beyond the sound's length
			ma_sound_set_at_end(&music, MA_TRUE);
		}
		else if (ma_sound_at_end(&music) && seekFrame < length)
		{
			// Sound reached the end but sought to an earlier frame
			ma_sound_set_at_end(&music, MA_FALSE);
		}
	}

	void AudioContext::setMusicOffset(float currentTime, float offset)
	{
		musicOffset = offset / 1000.0f;
		seekMusic(currentTime - musicOffset);

		float start = musicOffset + manager.getAudioEngineAbsoluteTime() - currentTime;
		ma_sound_set_start_time_in_milliseconds(&music, std::max(0.0f, start * 1000));
	}

	float AudioContext::getMusicPosition()
	{
		float cursor{};
		ma_sound_get_cursor_in_seconds(&music, &cursor);

		return cursor;
	}

	float AudioContext::getMusicLength()
	{
		float length{};
		ma_sound_get_length_in_seconds(&music, &length);

		return length;
	}

	float AudioContext::getMusicOffset() const { return musicOffset; }

	float AudioContext::getMusicEndTime()
	{
		float length = 0.0f;
		ma_sound_get_length_in_seconds(&music, &length);

		return length + musicOffset;
	}

	bool AudioContext::isMusicInitialized() const { return musicBuffer.isValid(); }

	bool AudioContext::isMusicAtEnd() const { return ma_sound_at_end(&music); }

	static mmw::Result decodeAudioFile(const std::string& filename, SoundBuffer& sound)
	{
		auto filepath = IO::stringToPath(filename);
		if (!IO::File::exists(filepath))
			return mmw::Result(mmw::ResultStatus::Error, "File not found");

		auto fileExtension = IO::toString(IO::File::getFileExtension(filepath));
		std::transform(fileExtension.begin(), fileExtension.end(), fileExtension.begin(),
		               ::tolower);

		if (!isSupportedFileFormat(fileExtension))
			return mmw::Result(mmw::ResultStatus::Error, "Unsupported file format");

		std::string nameWithoutExtension =
		    IO::toString(IO::File::getFilenameWithoutExtension(filepath));

		IO::File f(filename, IO::FileMode::ReadBinary);
		std::vector<uint8_t> bytes = f.readAllBytes();
		f.close();

		if (fileExtension == ".mp3")
		{
			ma_dr_mp3_config mp3Config{};
			uint64_t frameCount{};
			int16_t* samples = ma_dr_mp3_open_memory_and_read_pcm_frames_s16(
			    bytes.data(), bytes.size(), &mp3Config, &frameCount, nullptr);
			if (samples == nullptr)
				return mmw::Result(mmw::ResultStatus::Error, "Failed to decode mp3");

			sound.initialize(nameWithoutExtension, mp3Config.sampleRate, mp3Config.channels,
			                 frameCount, samples);
			return mmw::Result::Ok();
		}
		else if (fileExtension == ".wav")
		{
			uint32_t channels{};
			uint32_t sampleRate{};
			uint64_t frameCount{};
			int16_t* samples = ma_dr_wav_open_memory_and_read_pcm_frames_s16(
			    bytes.data(), bytes.size(), &channels, &sampleRate, &frameCount, nullptr);
			if (samples == nullptr)
				return mmw::Result(mmw::ResultStatus::Error, "Failed to decode wav");

			sound.initialize(nameWithoutExtension, sampleRate, channels, frameCount, samples);
			return mmw::Result::Ok();
		}
		else if (fileExtension == ".flac")
		{
			uint32_t channels{};
			uint32_t sampleRate{};
			uint64_t frameCount{};
			int16_t* samples = ma_dr_flac_open_memory_and_read_pcm_frames_s16(
			    bytes.data(), bytes.size(), &channels, &sampleRate, &frameCount, nullptr);
			if (samples == nullptr)
				return mmw::Result(mmw::ResultStatus::Error, "Failed to decode flac");

			sound.initialize(nameWithoutExtension, sampleRate, channels, frameCount, samples);
			return mmw::Result::Ok();
		}
		else if (fileExtension == ".ogg")
		{
			int32_t channels{};
			int32_t sampleRate{};
			int16_t* samples;
			int32_t frameCount = stb_vorbis_decode_memory(bytes.data(), bytes.size(), &channels,
			                                              &sampleRate, &samples);
			if (samples == nullptr)
				return mmw::Result(mmw::ResultStatus::Error, "Failed to decode ogg vorbis");

			sound.initialize(nameWithoutExtension, sampleRate, channels, frameCount, samples);
			return mmw::Result::Ok();
		}

		// Getting here should mean an unsupported file format
		return mmw::Result(mmw::ResultStatus::Error, "Unsupported file format");
	}

	mmw::Result AudioContext::loadMusic(const std::string& filename)
	{
		disposeMusic();
		mmw::Result result = decodeAudioFile(filename, musicBuffer);
		if (result.isOk())
		{
			// We want to always enable pitch here for miniaudio's resampler to work with playback
			// speed
			manager.initializeMusic(&music, &musicBuffer.buffer);

			// Sync
			setPlaybackSpeed(playbackSpeed, 0);
		}

		return result;
	}

	void AudioContext::disposeMusic()
	{
		if (musicBuffer.isValid())
		{
			ma_sound_stop(&music);
			ma_sound_uninit(&music);
			musicBuffer.dispose();
		}
	}

	void AudioContext::playOneShotSound(std::string_view name)
	{
		auto it = soundEffects->pool.find(name);
		if (it == soundEffects->pool.end())
			return;

		it->second->play(manager.getAudioEngineAbsoluteTime(), -1);
	}

	void AudioContext::playSoundEffect(std::string_view name, float start, float end,
	                                   float currentTime)
	{
		auto it = soundEffects->pool.find(name);
		if (it == soundEffects->pool.end())
			return;

		SoundPool& soundPool = *it->second;
		const float engineTime = manager.getAudioEngineAbsoluteTime();
		const float absoluteStart = absoluteStartTime + (start - startTime) / playbackSpeed;
		const float absoluteEnd = end < 0 ? -1 : ((end - start) / playbackSpeed) + absoluteStart;
		SoundInstance& currentInstance = soundPool.pool[soundPool.getCurrentIndex()];

		if (hasFlag(soundPool.flags, SoundFlags::EXTENDABLE))
		{
			// We want to re-use the currently playing instance

			const bool isCurrentInstancePlaying =
			    ma_node_get_state(&currentInstance.source) == ma_node_state_started;

			const bool isNewSoundWithinOldRange =
			    mmw::isWithinRange(absoluteStart, currentInstance.absoluteStart,
			                       currentInstance.absoluteEnd) &&
			    mmw::isWithinRange(absoluteEnd, currentInstance.absoluteStart,
			                       currentInstance.absoluteEnd);

			if (isNewSoundWithinOldRange && isCurrentInstancePlaying)
				return;

			if (isCurrentInstancePlaying && absoluteEnd > currentInstance.absoluteEnd)
			{
				currentInstance.extendDuration(absoluteEnd);
				return;
			}
		}

		soundPool.play(absoluteStart, absoluteEnd);
	}

	void AudioContext::stopSoundEffects(bool all)
	{
		if (soundEffects->pool.empty())
			return;
		if (all)
		{
			for (auto& [se, sound] : soundEffects->pool)
				sound->stopAll();
		}
		else
		{
			soundEffects->pool[mmw::SE_CONNECT]->stopAll();
			soundEffects->pool[mmw::SE_CRITICAL_CONNECT]->stopAll();

			// Also stop any scheduled sounds
			for (auto& [se, sound] : soundEffects->pool)
			{
				for (auto& instance : sound->pool)
				{
					ma_uint64 cursor{};
					ma_sound_get_cursor_in_pcm_frames(&instance.source, &cursor);
					if (cursor <= 0)
						ma_sound_stop(&instance.source);
				}
			}
		}
	}

	bool AudioContext::isSoundPlaying(std::string_view name) const
	{
		auto it = soundEffects->pool.find(name);
		if (it == soundEffects->pool.end())
			return false;

		return it->second->isAnyPlaying();
	}

	float AudioContext::getPlaybackSpeed() const { return playbackSpeed; }

	void AudioContext::setPlaybackSpeed(float speed, float currentTime)
	{
		const float engineTime = manager.getAudioEngineAbsoluteTime();
		const ma_uint32 speedAdjustedSampleRate =
		    static_cast<ma_uint32>(speed * musicBuffer.sampleRate);
		musicBuffer.effectiveSampleRate = speedAdjustedSampleRate;
		music.engineNode.sampleRate = speedAdjustedSampleRate;

		ma_uint32 sampleRateIn = speedAdjustedSampleRate;
		ma_uint32 sampleRateOut = manager.getAudioEngineSampleRate();
		ma_uint32 gcf = mmw::gcf(sampleRateIn, sampleRateOut);
		sampleRateIn /= gcf;
		sampleRateOut /= gcf;

		ma_linear_resampler& resampler = music.engineNode.resampler;
		resampler.lpf.sampleRate = std::max(sampleRateIn, sampleRateOut);
		resampler.inAdvanceInt = sampleRateIn / sampleRateOut;
		resampler.inAdvanceFrac = sampleRateIn % sampleRateOut;
		resampler.config.sampleRateIn = sampleRateIn;
		resampler.config.sampleRateOut = sampleRateOut;

		// Adjust timing of extendable sounds
		for (auto& [name, sound] : soundEffects->pool)
		{
			if (hasFlag(sound->flags, SoundFlags::EXTENDABLE))
			{
				for (auto& instance : sound->pool)
				{
					if (instance.isPlaying())
					{
						float newDuration =
						    (instance.absoluteEnd - engineTime) * playbackSpeed / speed;
						instance.extendDuration(engineTime + newDuration);
					}
				}
			}
		}

		playbackSpeed = speed;
		syncPlaybackTime(currentTime);
	}

	void AudioContext::syncPlaybackTime(float currentTime)
	{
		absoluteStartTime = manager.getAudioEngineAbsoluteTime();
		startTime = currentTime;
	}

	void AudioContext::syncSoundEffectsProfile()
	{
		if (soundEffectsProfileIndex == manager.getSoundEffectsProfileIndex())
			return;
		soundEffectsProfileIndex = manager.getSoundEffectsProfileIndex();
		soundEffects = manager.getCurrentSoundEffectsProfile();
	}
}
