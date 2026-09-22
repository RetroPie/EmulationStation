#include "resources/TextureDataManager.h"

#include "resources/TextureData.h"
#include "resources/TextureResource.h"
#include "Settings.h"

TextureDataManager::TextureDataManager()
{
	unsigned char data[5 * 5 * 4];
	mBlank = std::shared_ptr<TextureData>(new TextureData(false));
	for (int i = 0; i < (5 * 5); ++i)
	{
		data[i*4] = (i % 2) * 255;
		data[i*4+1] = (i % 2) * 255;
		data[i*4+2] = (i % 2) * 255;
		data[i*4+3] = 0;
	}
	mBlank->initFromRGBA(data, 5, 5);
	mLoader = new TextureLoader;
}

TextureDataManager::~TextureDataManager()
{
	delete mLoader;
}

std::shared_ptr<TextureData> TextureDataManager::add(const TextureResource* key, bool tiled)
{
	remove(key);
	std::shared_ptr<TextureData> data(new TextureData(tiled));
	mTextures.push_front(data);
	mTextureLookup[key] = mTextures.cbegin();
	return data;
}

void TextureDataManager::remove(const TextureResource* key)
{
	// Find the entry in the list
	auto it = mTextureLookup.find(key);
	if (it != mTextureLookup.cend())
	{
		// Cancel any pending async load for this texture
		mLoader->remove(*(*it).second);
		// Remove the list entry
		mTextures.erase((*it).second);
		// And the lookup
		mTextureLookup.erase(it);
	}
}

std::shared_ptr<TextureData> TextureDataManager::get(const TextureResource* key, bool enableLoading)
{
	// If it's in the cache then we want to remove it from it's current location and
	// move it to the top
	std::shared_ptr<TextureData> tex;
	auto it = mTextureLookup.find(key);
	if (it != mTextureLookup.cend())
	{
		tex = *(*it).second;
		// Remove the list entry
		mTextures.erase((*it).second);
		// Put it at the top
		mTextures.push_front(tex);
		// Store it back in the lookup
		mTextureLookup[key] = mTextures.cbegin();

		// Make sure it's loaded or queued for loading.
		// Skip textures whose load previously failed (e.g. file not found) so
		// we don't keep re-queuing them every frame they are rendered.
		if (enableLoading && tex->loadStatus() == TextureData::LoadStatus::LOADING)
			load(tex);
	}
	return tex;
}

bool TextureDataManager::bind(const TextureResource* key)
{
	std::shared_ptr<TextureData> tex = get(key);
	bool bound = false;
	if (tex != nullptr)
		bound = tex->uploadAndBind();
	if (!bound)
		mBlank->uploadAndBind();
	else
		// Stamp this texture as bound this generation so the eviction loop in
		// load() won't kick it out and cause it to flicker on the next frame.
		tex->setBindGeneration(mBindGeneration);
	return bound;
}

size_t TextureDataManager::getTotalSize()
{
	size_t total = 0;
	for (auto tex : mTextures)
	{
		// Only count textures whose dimensions are known — calling width()/height()
		// on an unloaded texture triggers a synchronous load().
		if (tex->loadStatus() == TextureData::LoadStatus::LOADED)
			total += tex->width() * tex->height() * 4;
	}
	return total;
}

size_t TextureDataManager::getCommittedSize()
{
	size_t total = 0;
	for (auto tex : mTextures)
		total += tex->getVRAMUsage();
	return total;
}

size_t TextureDataManager::getQueueSize()
{
	return mLoader->getQueueSize();
}

void TextureDataManager::load(std::shared_ptr<TextureData> tex, bool block)
{
	// See if it's already loaded or has permanently failed
	if (tex->loadStatus() != TextureData::LoadStatus::LOADING)
		return;
	// Not loaded. Make sure there is room
	size_t max_texture = (size_t)Settings::getInstance()->getInt("MaxVRAM") * 1024 * 1024;

	// if max_texture is 0, then texture memory should be considered unlimited
	if (max_texture > 0)
	{
		// Two-pass eviction. Pass 1 protects textures that were rendered in the
		// current or previous bind generation — evicting them would cause flickering
		// since they will be re-bound (and immediately re-loaded) on the very next
		// frame.  Pass 2 is a fallback that ignores the protection if we still don't
		// have enough budget after pass 1, so we never deadlock if the on-screen
		// working set genuinely exceeds MaxVRAM.
		const uint64_t protectGen = mBindGeneration > 0 ? mBindGeneration - 1 : 0;
		size_t size = TextureResource::getTotalMemUsage();
		for (auto it = mTextures.crbegin(); it != mTextures.crend(); ++it)
		{
			if (size < max_texture)
				break;
			// Only evict textures that actually have data in RAM or VRAM.
			// Evicting a LOADING texture frees no memory (getVRAMUsage()==0) and cancels
			// its pending background load — which then gets immediately re-queued on the
			// next bind(), triggering another eviction cycle. Skipping these breaks the
			// evict→cancel→re-queue→evict cascade that causes non-stop texture flickering.
			if ((*it)->loadStatus() != TextureData::LoadStatus::LOADED)
				continue;
			if ((*it)->bindGeneration() >= protectGen)
				continue;
			(*it)->releaseVRAM();
			(*it)->releaseRAM();
			size = TextureResource::getTotalMemUsage();
		}
		// Pass 2: if still over budget, evict on-screen textures too. Rare (only
		// when the working set itself is bigger than MaxVRAM), but necessary to
		// avoid runaway memory growth.
		if (size >= max_texture)
		{
			for (auto it = mTextures.crbegin(); it != mTextures.crend(); ++it)
			{
				if (size < max_texture)
					break;
				if ((*it)->loadStatus() != TextureData::LoadStatus::LOADED)
					continue;
				(*it)->releaseVRAM();
				(*it)->releaseRAM();
				size = TextureResource::getTotalMemUsage();
			}
		}
	}
	if (!block)
		mLoader->load(tex);
	else
		tex->load();
}

TextureLoader::TextureLoader() : mExit(false)
{
	mThread = new std::thread(&TextureLoader::threadProc, this);
}

TextureLoader::~TextureLoader()
{
	// Clear the queue and signal exit atomically under the mutex so there is no
	// race with the background thread's condition_variable wait (which holds the
	// mutex while sleeping).
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mTextureDataQ.clear();
		mTextureDataLookup.clear();
		mExit = true;
	}
	mEvent.notify_one();
	mThread->join();
	delete mThread;
}

void TextureLoader::threadProc()
{
	while (!mExit)
	{
		std::shared_ptr<TextureData> textureData;
		{
			// Wait for an event to say there is something in the queue
			std::unique_lock<std::mutex> lock(mMutex);
			mEvent.wait(lock);
			if (!mTextureDataQ.empty())
			{
				textureData = mTextureDataQ.front();
				mTextureDataQ.pop_front();
				mTextureDataLookup.erase(mTextureDataLookup.find(textureData.get()));
			}
		}
		// Queue has been released here but we might have a texture to process
		while (textureData)
		{
			textureData->load();

			// See if there is another item in the queue
			textureData = nullptr;
			std::unique_lock<std::mutex> lock(mMutex);
			if (!mTextureDataQ.empty())
			{
				textureData = mTextureDataQ.front();
				mTextureDataQ.pop_front();
				mTextureDataLookup.erase(mTextureDataLookup.find(textureData.get()));
			}
		}
	}
}

void TextureLoader::load(std::shared_ptr<TextureData> textureData)
{
	// Make sure it's not already loaded and hasn't permanently failed
	if (textureData->loadStatus() == TextureData::LoadStatus::LOADING)
	{
		std::unique_lock<std::mutex> lock(mMutex);
		// Remove it from the queue if it is already there
		auto td = mTextureDataLookup.find(textureData.get());
		if (td != mTextureDataLookup.cend())
		{
			mTextureDataQ.erase((*td).second);
			mTextureDataLookup.erase(td);
		}

		// Put it on the start of the queue as we want the newly requested textures to load first
		mTextureDataQ.push_front(textureData);
		mTextureDataLookup[textureData.get()] = mTextureDataQ.cbegin();
		mEvent.notify_one();
	}
}

void TextureLoader::remove(std::shared_ptr<TextureData> textureData)
{
	// Just remove it from the queue so we don't attempt to load it
	std::unique_lock<std::mutex> lock(mMutex);
	auto td = mTextureDataLookup.find(textureData.get());
	if (td != mTextureDataLookup.cend())
	{
		mTextureDataQ.erase((*td).second);
		mTextureDataLookup.erase(td);
	}
}

size_t TextureLoader::getQueueSize()
{
	// Gets the amount of video memory that will be used once all textures in
	// the queue are loaded.  Only count textures whose dimensions are already
	// known — calling width()/height() on an unloaded texture triggers a
	// synchronous load() which blocks the main thread on NAS I/O.
	size_t mem = 0;
	std::unique_lock<std::mutex> lock(mMutex);
	for (auto tex : mTextureDataQ)
	{
		if (tex->loadStatus() == TextureData::LoadStatus::LOADED)
			mem += tex->width() * tex->height() * 4;
	}
	return mem;
}
