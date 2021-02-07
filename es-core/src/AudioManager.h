#pragma once
#ifndef ES_CORE_AUDIO_MANAGER_H
#define ES_CORE_AUDIO_MANAGER_H

#include <SDL_audio.h>
#include <memory>
#include <vector>
#include "SDL_mixer.h"
#include <string>
#include <iostream>

class Sound;

class AudioManager
{
private:
	AudioManager();

	static std::vector<std::shared_ptr<Sound>> sSoundVector;
	static AudioManager* sInstance;

	Mix_Music* mCurrentMusic;
	void getMusicIn(const std::string &path, std::vector<std::string>& all_matching_files);
	void playMusic(std::string path);
	static void musicEnd_callback();

	bool mInitialized;

public:
	static AudioManager* getInstance();
	static bool isInitialized();

	void init();
	void deinit();

	void registerSound(std::shared_ptr<Sound> & sound);
	void unregisterSound(std::shared_ptr<Sound> & sound);

	void play();
	void stop();

	// RetroPie Mixer
	void playRandomMusic(bool continueIfPlaying = true);
	void stopMusic(bool fadeOut = true);

	virtual ~AudioManager();

	float mMusicVolume;
	static void update(int deltaTime);
	static int getMaxMusicVolume();
};

#endif // ES_CORE_AUDIO_MANAGER_H
