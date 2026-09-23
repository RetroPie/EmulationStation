#pragma once
#ifndef ES_CORE_RESOURCES_TEXTURE_DATA_H
#define ES_CORE_RESOURCES_TEXTURE_DATA_H

#include <cstdint>
#include <mutex>
#include <string>

class TextureResource;

class TextureData
{
public:
	TextureData(bool tile);
	~TextureData();

	// These functions populate mDataRGBA but do not upload the texture to VRAM

	//!!!! Needs to be canonical path. Caller should check for duplicates before calling this
	void initFromPath(const std::string& path);
	bool initSVGFromMemory(const unsigned char* fileData, size_t length);
	bool initImageFromMemory(const unsigned char* fileData, size_t length);
	bool initFromRGBA(const unsigned char* dataRGBA, size_t width, size_t height);

	enum class LoadStatus
	{
		LOADING,  // not yet loaded or in progress
		LOADED,   // pixel data is in RAM or VRAM
		FAILED    // load() was attempted and permanently failed (e.g. file not found)
	};

	// Read the data into memory if necessary
	bool load();

	// Returns the current load state under a single mutex acquisition.
	LoadStatus loadStatus();

	// Upload the texture to VRAM if necessary and bind. Returns true if bound ok or
	// false if either not loaded
	bool uploadAndBind();

	// Release the texture from VRAM
	void releaseVRAM();

	// Release the texture from conventional RAM
	void releaseRAM();

	// Get the amount of VRAM currenty used by this texture
	size_t getVRAMUsage();

	size_t width();
	size_t height();
	float sourceWidth();
	float sourceHeight();
	void setSourceSize(float width, float height);

	bool tiled() { return mTile; }

	// Bind generation when this texture was last successfully bound. Compared
	// against the manager's current generation by the eviction loop, so that
	// textures rendered in recent frames are protected from being evicted.
	uint64_t bindGeneration() const { return mBindGeneration; }
	void setBindGeneration(uint64_t gen) { mBindGeneration = gen; }

private:
	std::mutex		mMutex;
	bool			mTile;
	std::string		mPath;
	unsigned int	mTextureID;
	unsigned char*	mDataRGBA;
	size_t			mWidth;
	size_t			mHeight;
	float			mSourceWidth;
	float			mSourceHeight;
	bool			mScalable;
	bool			mReloadable;
	bool			mLoadFailed;
	uint64_t		mBindGeneration;
};

#endif // ES_CORE_RESOURCES_TEXTURE_DATA_H
