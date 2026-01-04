#include "audio/BackgroundMusicManager.h"

#include "Settings.h"
#include "Log.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <SDL.h>
#include <SDL_mixer.h>

#include <algorithm>
#include <random>
#include <cmath>

// =============================
//  Singleton
// =============================

BackgroundMusicManager* BackgroundMusicManager::sInstance = nullptr;

BackgroundMusicManager& BackgroundMusicManager::getInstance()
{
    if (!sInstance)
        sInstance = new BackgroundMusicManager();
    return *sInstance;
}

// =============================
//  Ctor / Dtor
// =============================

BackgroundMusicManager::BackgroundMusicManager()
    : mInitialized(false)
    , mEnabled(true)
    , mGameRunning(false)
    , mMixerOpenedByUs(false)
    , mCurrentIndex(-1)
    , mCurrentMusic(nullptr)
    , mNowPlayingText("")
    , mSongNameChanged(false)
    , mPendingResume(false)
    , mResumeDelayMs(600)
    , mResumeTimerMs(0)
    , mPendingIndex(-1)
    , mPendingReopenOnResume(false)
    , mPendingNextFromCallback(false)
{
    mEnabled = Settings::getInstance()->getBool("BackgroundMusic");
}

BackgroundMusicManager::~BackgroundMusicManager()
{
    shutdown();
}

// =============================
//  Popup helpers
// =============================

bool BackgroundMusicManager::songNameChanged()
{
    return mSongNameChanged;
}

void BackgroundMusicManager::resetSongNameChangedFlag()
{
    mSongNameChanged = false;
}

std::string BackgroundMusicManager::getCurrentSongDisplayName() const
{
    return mNowPlayingText;
}

void BackgroundMusicManager::setNowPlaying(const std::string& fullPath)
{
    std::string stem = Utils::FileSystem::getStem(fullPath);
    if (stem.empty())
        stem = fullPath;

    mNowPlayingText = std::string("🎵 Now playing: ") + stem;
    mSongNameChanged = true;
}

// =============================
//  Mixer open/close/reopen
// =============================

bool BackgroundMusicManager::openMixer()
{
    if (mMixerOpenedByUs)
        return true;

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        LOG(LogError) << "BGM - Mix_OpenAudio failed: " << Mix_GetError();
        return false;
    }

    Mix_AllocateChannels(16);
    Mix_HaltMusic();
    Mix_HookMusicFinished(&BackgroundMusicManager::musicFinishedCallbackStatic);

    mMixerOpenedByUs = true;
    LOG(LogInfo) << "BGM - Mixer OPEN";
    return true;
}

void BackgroundMusicManager::closeMixer()
{
    if (!mMixerOpenedByUs)
        return;

    mPendingResume = false;
    mResumeTimerMs = 0;
    mPendingIndex  = -1;
    mPendingReopenOnResume = false;
    mPendingNextFromCallback.store(false);

    stopMusicInternal(false);
    Mix_HookMusicFinished(nullptr);
    Mix_HaltMusic();
    Mix_CloseAudio();

    mMixerOpenedByUs = false;
    LOG(LogInfo) << "BGM - Mixer CLOSED";
}

bool BackgroundMusicManager::reopenMixer()
{
    if (mMixerOpenedByUs)
    {
        stopMusicInternal(false);
        Mix_HookMusicFinished(nullptr);
        Mix_HaltMusic();
        Mix_CloseAudio();
        mMixerOpenedByUs = false;
        LOG(LogInfo) << "BGM - Mixer REOPEN (close)";
    }

    return openMixer();
}

// =============================
//  Init / Shutdown
// =============================

void BackgroundMusicManager::init()
{
    if (mInitialized)
        return;

    if (!openMixer())
        return;

    buildPlaylist();
    mInitialized = true;

    LOG(LogInfo) << "BGM - Initialized | playlist=" << mPlaylist.size()
                 << " | enabled=" << (mEnabled ? "1" : "0");

    if (mEnabled && !mPlaylist.empty())
        playCurrent();
}

void BackgroundMusicManager::shutdown()
{
    closeMixer();

    mInitialized = false;
    mPlaylist.clear();
    mLastPlayed.clear();
    mCurrentIndex = -1;

    LOG(LogInfo) << "BGM - Shutdown";
}

// =============================
//  Enable / Disable
// =============================

void BackgroundMusicManager::setEnabled(bool enabled)
{
    if (enabled == mEnabled)
        return;

    mEnabled = enabled;
    Settings::getInstance()->setBool("BackgroundMusic", enabled);

    LOG(LogInfo) << "BGM - setEnabled(" << (enabled ? "true" : "false") << ")";

    if (!mEnabled)
    {
        mPendingResume = false;
        mResumeTimerMs = 0;
        mPendingIndex  = -1;
        mPendingReopenOnResume = false;
        mPendingNextFromCallback.store(false);

        stopMusicInternal(true);
        return;
    }

    if (!mInitialized)
    {
        init();
        return;
    }

    if (!openMixer())
        return;

    if (mPlaylist.empty())
        buildPlaylist();

    if (!mPlaylist.empty())
        playCurrent();
}

// =============================
//  Game hooks
// =============================

void BackgroundMusicManager::onGameLaunched()
{
    if (!mInitialized)
        return;

    mGameRunning = true;

    mPendingResume = false;
    mResumeTimerMs = 0;
    mPendingIndex  = -1;
    mPendingNextFromCallback.store(false);

    LOG(LogInfo) << "BGM - onGameLaunched() -> stop music";
    stopMusicInternal(true);
}

void BackgroundMusicManager::onGameEnded()
{
    mGameRunning = false;

    if (!mEnabled || !mInitialized)
        return;

    if (!mPlaylist.empty())
        mPendingIndex = pickNextIndex();
    else
        mPendingIndex = -1;

    mPendingReopenOnResume = true;
    mPendingResume = true;
    mResumeTimerMs = mResumeDelayMs;

    LOG(LogInfo) << "BGM - onGameEnded(): resume in " << mResumeDelayMs << "ms";
}

// =============================
//  Update (main thread)
// =============================

void BackgroundMusicManager::update(int deltaTimeMs)
{
    if (!mInitialized)
        return;

    if (mPendingNextFromCallback.exchange(false))
    {
        if (!mGameRunning && mEnabled)
            playNext();
    }

    if (!mPendingResume)
        return;

    if (mGameRunning || !mEnabled)
    {
        mPendingResume = false;
        mResumeTimerMs = 0;
        mPendingIndex  = -1;
        mPendingReopenOnResume = false;
        return;
    }

    mResumeTimerMs -= deltaTimeMs;
    if (mResumeTimerMs > 0)
        return;

    mPendingResume = false;
    mResumeTimerMs = 0;

    if (mPlaylist.empty())
        buildPlaylist();

    if (mPlaylist.empty())
        return;

    if (mPendingReopenOnResume)
    {
        mPendingReopenOnResume = false;
        if (!reopenMixer())
            return;
    }
    else
    {
        if (!openMixer())
            return;
    }

    if (mPendingIndex >= 0 && mPendingIndex < (int)mPlaylist.size())
        mCurrentIndex = mPendingIndex;
    else
        mCurrentIndex = pickNextIndex();

    mPendingIndex = -1;

    LOG(LogInfo) << "BGM - resume -> playCurrent() index=" << mCurrentIndex;
    playCurrent();
}

// =============================
//  Playlist
// =============================

void BackgroundMusicManager::buildPlaylist()
{
    mPlaylist.clear();
    mLastPlayed.clear();
    mCurrentIndex = -1;

    const std::string home = Utils::FileSystem::getHomePath();
    buildPlaylistFromPath(home + "/RetroPie/music");
    buildPlaylistFromPath(home + "/.emulationstation/music");

    if (!mPlaylist.empty())
    {
        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_int_distribution<int> dist(0, (int)mPlaylist.size() - 1);
        mCurrentIndex = dist(rng);

        LOG(LogInfo) << "BGM - Playlist ready (" << mPlaylist.size() << " tracks)";
    }
}

void BackgroundMusicManager::buildPlaylistFromPath(const std::string& path)
{
    if (!Utils::FileSystem::exists(path) || !Utils::FileSystem::isDirectory(path))
        return;

    for (const auto& e : Utils::FileSystem::getDirContent(path))
        if (!Utils::FileSystem::isDirectory(e) && isValidAudioFile(e))
            mPlaylist.push_back(e);
}

bool BackgroundMusicManager::isValidAudioFile(const std::string& path) const
{
    const std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(path));
    return (ext == ".mp3" || ext == ".ogg" || ext == ".flac" || ext == ".wav" || ext == ".mod");
}

// =============================
//  Shuffle inteligente
// =============================

void BackgroundMusicManager::addLastPlayed(const std::string& song)
{
    int maxHistory = std::max(1, (int)std::floor(mPlaylist.size() * 0.4f));

    mLastPlayed.push_front(song);
    while ((int)mLastPlayed.size() > maxHistory)
        mLastPlayed.pop_back();
}

bool BackgroundMusicManager::wasPlayedRecently(const std::string& song) const
{
    return std::find(mLastPlayed.begin(), mLastPlayed.end(), song) != mLastPlayed.end();
}

int BackgroundMusicManager::pickNextIndex()
{
    if (mPlaylist.size() <= 1)
        return mCurrentIndex;

    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(0, (int)mPlaylist.size() - 1);

    int tries = 0;
    int idx = mCurrentIndex;

    do {
        idx = dist(rng);
        tries++;
    } while (tries < 20 && wasPlayedRecently(mPlaylist[idx]));

    return idx;
}

// =============================
//  Playback
// =============================

void BackgroundMusicManager::playCurrent()
{
    if (!mInitialized || !mEnabled || !mMixerOpenedByUs || mPlaylist.empty())
        return;

    if (Mix_PlayingMusic() && !Mix_PausedMusic())
        return;

    int attempts = 0;
    int maxAttempts = std::min((int)mPlaylist.size(), 10);

    while (attempts < maxAttempts && !mPlaylist.empty())
    {
        if (mCurrentIndex < 0 || mCurrentIndex >= (int)mPlaylist.size())
            mCurrentIndex = pickNextIndex();

        const std::string song = mPlaylist[mCurrentIndex];

        if (mCurrentMusic)
        {
            Mix_HaltMusic();
            Mix_FreeMusic(mCurrentMusic);
            mCurrentMusic = nullptr;
        }

        mCurrentMusic = Mix_LoadMUS(song.c_str());
        if (!mCurrentMusic)
        {
            LOG(LogError) << "BGM - Load failed, skipping: " << song
                          << " (" << Mix_GetError() << ")";

            mPlaylist.erase(mPlaylist.begin() + mCurrentIndex);
            mCurrentIndex = -1;
            attempts++;
            continue;
        }

        LOG(LogInfo) << "BGM - Playing: " << song;
        setNowPlaying(song);

        Mix_PlayMusic(mCurrentMusic, 0);
        addLastPlayed(song);
        return;
    }

    LOG(LogWarning) << "BGM - No valid tracks left, stopping music";
}

// =============================
//  Controls
// =============================

void BackgroundMusicManager::playNext()
{
    if (!mInitialized || mPlaylist.empty())
        return;

    mCurrentIndex = pickNextIndex();
    playCurrent();
}

void BackgroundMusicManager::stopMusicInternal(bool fadeOut)
{
    if (!mMixerOpenedByUs)
        return;

    if (fadeOut)
        Mix_FadeOutMusic(500);
    else
        Mix_HaltMusic();

    if (mCurrentMusic)
    {
        Mix_FreeMusic(mCurrentMusic);
        mCurrentMusic = nullptr;
    }
}

// =============================
//  Callback
// =============================

void BackgroundMusicManager::musicFinishedCallbackStatic()
{
    if (sInstance)
        sInstance->musicFinishedCallback();
}

void BackgroundMusicManager::musicFinishedCallback()
{
    if (mGameRunning || !mEnabled)
        return;

    mPendingNextFromCallback.store(true);
}
