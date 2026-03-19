#pragma once
#include "Sound.h"
#include <unordered_map>
#include <vector>
#include <array>
#include <memory>

namespace Audio
{
	class AudioManager
	{
	  private:
		ma_engine engine;
		ma_sound_group musicGroup;
		ma_sound_group soundEffectsGroup;
		std::shared_ptr<SoundEffectProfile> sounds;

		float masterVolume{ 1.0f };
		float musicVolume{ 1.0f };
		float soundEffectsVolume{ 1.0f };

		int soundEffectsProfileIndex{ 0 };

	  public:
		std::vector<SoundInstance> baseSounds;

		void initializeAudioEngine();
		void uninitializeAudioEngine();
		void startEngine();
		void stopEngine();
		bool isEngineStarted() const;
		void loadSoundEffects();

		void initializeMusic(ma_sound* music, ma_audio_buffer* buffer);
		std::shared_ptr<SoundEffectProfile> getCurrentSoundEffectsProfile();

		uint32_t getDeviceSampleRate() const;
		uint32_t getDeviceChannelCount() const;
		float getDeviceLatency() const;

		float getAudioEngineAbsoluteTime() const;
		void setAudioEngineAbsoluteTime(float time);
		uint32_t getAudioEngineSampleRate() const;

		void setMasterVolume(float volume);
		float getMasterVolume() const;

		void setMusicVolume(float volume);
		float getMusicVolume() const;

		void setSoundEffectsVolume(float volume);
		float getSoundEffectsVolume() const;

		int getSoundEffectsProfileIndex() const;
		void setSoundEffectsProfileIndex(int index);

	  private:
		void initializeCurrentSoundProfile();
		void uninitializeSoundProfile();
	};

	class AudioContext
	{
		AudioManager& manager;
		ma_sound music;
		std::shared_ptr<SoundEffectProfile> soundEffects;

		// Offset from chart time in seconds
		// Positive is late, negative is early
		float musicOffset{ 0.0f };

		float playbackSpeed{ 1.0f };
		int soundEffectsProfileIndex{ 0 };

		float absoluteStartTime{};
		float startTime{};

	  public:
		SoundBuffer musicBuffer;
		AudioContext(AudioManager& manager);
		~AudioContext();

		void playMusic(float currentTime);
		void stopMusic();
		void seekMusic(float musicTime);
		void setMusicOffset(float currentTime, float offset);
		float getMusicPosition();
		float getMusicLength();
		float getMusicOffset() const;
		float getMusicEndTime();
		bool isMusicInitialized() const;
		bool isMusicAtEnd() const;

		MikuMikuWorld::Result loadMusic(const std::string& filename);
		void disposeMusic();

		void playOneShotSound(std::string_view name);
		void playSoundEffect(std::string_view name, float start, float end, float currentTime);
		void stopSoundEffects(bool all);
		bool isSoundPlaying(std::string_view name) const;

		float getPlaybackSpeed() const;
		void setPlaybackSpeed(float speed, float currentTime);

		void syncPlaybackTime(float currentTime);
		void syncSoundEffectsProfile();
	};
}