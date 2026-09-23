#pragma once
#ifndef ES_CORE_COMPONENTS_VIDEO_VLC_COMPONENT_H
#define ES_CORE_COMPONENTS_VIDEO_VLC_COMPONENT_H

#include "VideoComponent.h"
#include <functional>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>

struct SDL_mutex;
struct SDL_Surface;
struct libvlc_instance_t;
struct libvlc_media_t;
struct libvlc_media_player_t;

struct VideoContext {
	SDL_Surface*		surface;
	SDL_mutex*			mutex;
	bool				valid;
};

class VideoVlcComponent : public VideoComponent
{
	// Structure that groups together the configuration of the video component
	struct Configuration
	{
		unsigned						startDelay;
		bool							showSnapshotNoVideo;
		bool							showSnapshotDelay;
		std::string						defaultVideoPath;
	};

public:
	static void setupVLC(std::string subtitles);

	VideoVlcComponent(Window* window, std::string subtitles);
	virtual ~VideoVlcComponent();

	void render(const Transform4x4f& parentTrans) override;


	// Resize the video to fit this size. If one axis is zero, scale that axis to maintain aspect ratio.
	// If both are non-zero, potentially break the aspect ratio.  If both are zero, no resizing.
	// Can be set before or after a video is loaded.
	// setMaxSize() and setResize() are mutually exclusive.
	void setResize(float width, float height) override;

	// Resize the video to be as large as possible but fit within a box of this size.
	// Can be set before or after a video is loaded.
	// Never breaks the aspect ratio. setMaxSize() and setResize() are mutually exclusive.
	void setMaxSize(float width, float height) override;

	// Signal the cleanup worker to exit and wait for it to finish.
	// Must be called after all VideoVlcComponent instances are destroyed.
	static void deinit();

private:
	// Calculates the correct mSize from our resizing information (set by setResize/setMaxSize).
	// Used internally whenever the resizing parameters or texture change.
	void resize();
	// Start the video Immediately
	virtual void startVideo() override;
	// Stop the video
	virtual void stopVideo() override;
	// Handle looping the video. Must be called periodically
	virtual void handleLooping() override;

	void setMuteMode();
	void setupContext();
	void freeContext();

	// Called each frame to check if async media parsing has completed;
	// once done, extracts track info and starts playback.
	void handleParsing();
	// Second half of startVideo — runs after parsing finishes.
	void onMediaParsed();

	// Post a cleanup task to the shared background worker thread.
	static void postCleanupTask(std::function<void()> task);

	static void cleanupWorker();

private:
	static libvlc_instance_t*		mVLC;
	static std::thread				sCleanupThread;
	static std::mutex				sCleanupMutex;
	static std::condition_variable	sCleanupCond;
	static std::deque<std::function<void()>> sCleanupQueue;
	static bool						sCleanupRunning;
	static bool						sCleanupExit;
	libvlc_media_t*					mMedia;
	libvlc_media_player_t*			mMediaPlayer;
	VideoContext*				mContext;
	std::shared_ptr<TextureResource> mTexture;
	bool							mMediaParsing;
};

#endif // ES_CORE_COMPONENTS_VIDEO_VLC_COMPONENT_H
