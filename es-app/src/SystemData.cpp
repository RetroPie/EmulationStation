#include "SystemData.h"

#include "utils/FileSystemUtil.h"
#include "CollectionSystemManager.h"
#include "FileFilterIndex.h"
#include "FileSorts.h"
#include "Gamelist.h"
#include "Log.h"
#include "platform.h"
#include "Settings.h"
#include "ThemeData.h"
#include "views/UIModeController.h"
#include <fstream>
#include <random>
#include "utils/StringUtil.h"
#include "utils/ThreadPool.h"
#include "Window.h"

using namespace Utils;

std::vector<SystemData*> SystemData::sSystemVector;
std::vector<SystemData*> SystemData::sSystemVectorShuffled;
std::ranlux48 SystemData::sURNG = std::ranlux48(std::random_device()());

namespace
{
	// Trunca el nombre a maxLen caracteres, agregando "..." si hace falta
	std::string truncateMostPlayedName(const std::string& name, size_t maxLen)
	{
		if (name.size() <= maxLen)
			return name;

		if (maxLen <= 3)
			return name.substr(0, maxLen);

		return name.substr(0, maxLen - 3) + "...";
	}

	static std::string expandHomeTilde(const std::string& p)
	{
		if (p.empty()) return p;
		if (p[0] != '~') return p;
		if (p.size() == 1) return Utils::FileSystem::getHomePath();
		if (p[1] == '/') return Utils::FileSystem::getHomePath() + p.substr(1);
		return p;
	}

	static bool fileExistsAny(const std::string& p)
	{
		return (!p.empty() && Utils::FileSystem::exists(p));
	}
}

SystemData::SystemData(const std::string& name, const std::string& fullName, SystemEnvironmentData* envData, const std::string& themeFolder, bool CollectionSystem) :
	mName(name), mFullName(fullName), mEnvData(envData), mThemeFolder(themeFolder), mIsCollectionSystem(CollectionSystem), mIsGameSystem(true)
{
	mFilterIndex = new FileFilterIndex();

	// if it's an actual system, initialize it, if not, just create the data structure
	if(!CollectionSystem)
	{
		mRootFolder = new FileData(FOLDER, mEnvData->mStartPath, mEnvData, this);
		mRootFolder->metadata.set("name", mFullName);

		if(!Settings::getInstance()->getBool("ParseGamelistOnly"))
			populateFolder(mRootFolder);

		if(!Settings::getInstance()->getBool("IgnoreGamelist"))
			parseGamelist(this);

		mRootFolder->sort(FileSorts::SortTypes.at(0));

		indexAllGameFilters(mRootFolder);
	}
	else
	{
		// virtual systems are updated afterwards, we're just creating the data structure
		mRootFolder = new FileData(FOLDER, "" + name, mEnvData, this);
	}
	setIsGameSystemStatus();
	loadTheme();
}

SystemData::~SystemData()
{
	if(Settings::getInstance()->getString("SaveGamelistsMode") == "on exit")
		writeMetaData();

	delete mRootFolder;
	delete mFilterIndex;
}

void SystemData::setIsGameSystemStatus()
{
	// we exclude non-game systems from specific operations (i.e. the "RetroPie" system, at least)
	// if/when there are more in the future, maybe this can be a more complex method, with a proper list
	// but for now a simple string comparison is more performant
	mIsGameSystem = (mName != "retropie");
}

void SystemData::populateFolder(FileData* folder)
{
	const std::string& folderPath = folder->getPath();
	if(!Utils::FileSystem::isDirectory(folderPath))
	{
		LOG(LogWarning) << "Error - folder with path \"" << folderPath << "\" is not a directory!";
		return;
	}

	//make sure that this isn't a symlink to a thing we already have
	if(Utils::FileSystem::isSymlink(folderPath))
	{
		//if this symlink resolves to somewhere that's at the beginning of our path, it's gonna recurse
		if(folderPath.find(Utils::FileSystem::getCanonicalPath(folderPath)) == 0)
		{
			LOG(LogWarning) << "Skipping infinitely recursive symlink \"" << folderPath << "\"";
			return;
		}
	}

	std::string filePath;
	std::string extension;
	bool isGame;
	bool showHidden = Settings::getInstance()->getBool("ShowHiddenFiles");
	Utils::FileSystem::stringList dirContent = Utils::FileSystem::getDirContent(folderPath);
	for(Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it)
	{
		filePath = *it;

		// skip hidden files and folders
		if(!showHidden && Utils::FileSystem::isHidden(filePath))
			continue;

		//this is a little complicated because we allow a list of extensions to be defined (delimited with a space)
		//we first get the extension of the file itself:
		extension = Utils::FileSystem::getExtension(filePath);

		//fyi, folders *can* also match the extension and be added as games - this is mostly just to support higan
		//see issue #75: https://github.com/Aloshi/EmulationStation/issues/75

		isGame = false;
		if(std::find(mEnvData->mSearchExtensions.cbegin(), mEnvData->mSearchExtensions.cend(), extension) != mEnvData->mSearchExtensions.cend())
		{
			FileData* newGame = new FileData(GAME, filePath, mEnvData, this);

			// preventing new arcade assets to be added
			if(!newGame->isArcadeAsset())
			{
				folder->addChild(newGame);
				isGame = true;
			}
		}

		//add directories that also do not match an extension as folders
		if(!isGame && Utils::FileSystem::isDirectory(filePath))
		{
			FileData* newFolder = new FileData(FOLDER, filePath, mEnvData, this);
			populateFolder(newFolder);

			//ignore folders that do not contain games
			if(newFolder->getChildrenByFilename().size() == 0)
				delete newFolder;
			else
				folder->addChild(newFolder);
		}
	}
}

void SystemData::indexAllGameFilters(const FileData* folder)
{
	const std::vector<FileData*>& children = folder->getChildren();

	for(std::vector<FileData*>::const_iterator it = children.cbegin(); it != children.cend(); ++it)
	{
		switch((*it)->getType())
		{
			case GAME:   { mFilterIndex->addToIndex(*it); } break;
			case FOLDER: { indexAllGameFilters(*it);      } break;
			default:
				LOG(LogInfo) << "Unknown type: " << (*it)->getType();
			     break;
		}
	}
}

std::vector<std::string> readList(const std::string& str, const char* delims = " \t\r\n,")
{
	std::vector<std::string> ret;

	size_t prevOff = str.find_first_not_of(delims, 0);
	size_t off = str.find_first_of(delims, prevOff);
	while(off != std::string::npos || prevOff != std::string::npos)
	{
		ret.push_back(str.substr(prevOff, off - prevOff));

		prevOff = str.find_first_not_of(delims, off);
		off = str.find_first_of(delims, prevOff);
	}

	return ret;
}

SystemData* SystemData::loadSystem(pugi::xml_node system)
{
	std::string name, fullname, path, cmd, themeFolder, defaultCore;

	name = system.child("name").text().get();
	fullname = system.child("fullname").text().get();
	path = system.child("path").text().get();
	defaultCore = system.child("defaultCore").text().get();

	std::vector<std::string> list = readList(system.child("extension").text().get());
	std::vector<std::string> extensions;

	for (auto extension = list.cbegin(); extension != list.cend(); extension++)
	{
		std::string xt = std::string(*extension);
		if (std::find(extensions.begin(), extensions.end(), xt) == extensions.end())
			extensions.push_back(xt);
	}

	cmd = system.child("command").text().get();

	// platform id list
	const char* platformList = system.child("platform").text().get();
	std::vector<std::string> platformStrs = readList(platformList);
	std::vector<PlatformIds::PlatformId> platformIds;
	for (auto it = platformStrs.cbegin(); it != platformStrs.cend(); it++)
	{
		const char* str = it->c_str();
		PlatformIds::PlatformId platformId = PlatformIds::getPlatformId(str);

		if (platformId == PlatformIds::PLATFORM_IGNORE)
		{
			// when platform is ignore, do not allow other platforms
			platformIds.clear();
			platformIds.push_back(platformId);
			break;
		}

		// if there appears to be an actual platform ID supplied but it didn't match the list, warn
		if (str != NULL && str[0] != '\0' && platformId == PlatformIds::PLATFORM_UNKNOWN)
		{
			LOG(LogWarning) << "  Unknown platform for system \"" << name << "\" (platform \"" << str << "\" from list \"" << platformList << "\")";
		}
		else if (platformId != PlatformIds::PLATFORM_UNKNOWN)
			platformIds.push_back(platformId);
	}

	// theme folder
	themeFolder = system.child("theme").text().as_string(name.c_str());

	//validate
	if (name.empty() || path.empty() || extensions.empty() || cmd.empty())
	{
		LOG(LogError) << "System \"" << name << "\" is missing name, path, extension, or command!";
		return nullptr;
	}

	//convert path to generic directory seperators
	path = Utils::FileSystem::getGenericPath(path);

	//expand home symbol if the startpath contains ~
	if (path[0] == '~')
	{
		path.erase(0, 1);
		path.insert(0, Utils::FileSystem::getHomePath());
	}

	//create the system runtime environment data
	SystemEnvironmentData* envData = new SystemEnvironmentData;
	envData->mStartPath = path;
	envData->mSearchExtensions = extensions;
	envData->mLaunchCommand = cmd;
	envData->mPlatformIds = platformIds;

	SystemData* newSys = new SystemData(name, fullname, envData, themeFolder);
	if (newSys->getRootFolder()->getChildren().size() == 0)
	{
		LOG(LogWarning) << "System \"" << name << "\" has no games! Ignoring it.";
		delete newSys;

		return nullptr;
	}

	return newSys;
}

//creates systems from information located in a config file
bool SystemData::loadConfig(Window* window)
{
	deleteSystems();

	std::string path = getConfigPath(false);

	LOG(LogInfo) << "Loading system config file " << path << "...";

	if (!Utils::FileSystem::exists(path))
	{
		LOG(LogError) << "es_systems.cfg file does not exist!";
		writeExampleConfig(getConfigPath(true));
		return false;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());

	if(!res)
	{
		LOG(LogError) << "Could not parse es_systems.cfg file!";
		LOG(LogError) << res.description();
		return false;
	}

	//actually read the file
	pugi::xml_node systemList = doc.child("systemList");

	if(!systemList)
	{
		LOG(LogError) << "es_systems.cfg is missing the <systemList> tag!";
		return false;
	}

	std::vector<std::string> systemsNames;

	int systemCount = 0;
	for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		systemsNames.push_back(system.child("fullname").text().get());
		systemCount++;
	}

	int currentSystem = 0;

	typedef SystemData* SystemDataPtr;

	ThreadPool* pThreadPool = NULL;
	SystemDataPtr* systems = NULL;

	if (std::thread::hardware_concurrency() > 2 && Settings::getInstance()->getBool("ThreadedLoading"))
	{
		pThreadPool = new ThreadPool();

		systems = new SystemDataPtr[systemCount];
		for (int i = 0; i < systemCount; i++)
			systems[i] = nullptr;

		pThreadPool->queueWorkItem([] { CollectionSystemManager::get()->loadCollectionSystems(true); });
	}

	int processedSystem = 0;

	for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		if (pThreadPool != NULL)
		{
			pThreadPool->queueWorkItem([system, currentSystem, systems, &processedSystem]
			{
				systems[currentSystem] = loadSystem(system);
				processedSystem++;
			});
		}
		else
		{
			std::string fullname = system.child("fullname").text().get();

			if (window != NULL)
				window->renderLoadingScreen(fullname, systemCount == 0 ? 0 : (float)currentSystem / (float)(systemCount + 1));

			std::string nm = system.child("name").text().get();

			SystemData* pSystem = loadSystem(system);
			if (pSystem != nullptr)
				sSystemVector.push_back(pSystem);
		}

		currentSystem++;
	}

	if (pThreadPool != NULL)
	{
		if (window != NULL)
		{
			pThreadPool->wait([window, &processedSystem, systemCount, &systemsNames]
			{
				int px = processedSystem - 1;
				if (px >= 0 && px < (int)systemsNames.size())
					window->renderLoadingScreen(systemsNames.at(px), (float)px / (float)(systemCount + 1));
			}, 10);
		}
		else
			pThreadPool->wait();

		for (int i = 0; i < systemCount; i++)
		{
			SystemData* pSystem = systems[i];
			if (pSystem != nullptr)
				sSystemVector.push_back(pSystem);
		}

		delete[] systems;
		delete pThreadPool;

		if (window != NULL)
			window->renderLoadingScreen("Favorites", systemCount == 0 ? 0 : (float)currentSystem / (float)systemCount);

		CollectionSystemManager::get()->updateSystemsList();
	}
	else
	{
		if (window != NULL)
			window->renderLoadingScreen("Favorites", systemCount == 0 ? 0 : (float)currentSystem / (float)systemCount);

		CollectionSystemManager::get()->loadCollectionSystems();
	}

	return true;
}

void SystemData::writeExampleConfig(const std::string& path)
{
	std::ofstream file(path.c_str());

	file << "<!-- This is the EmulationStation Systems configuration file.\n"
			"All systems must be contained within the <systemList> tag.-->\n"
			"\n"
			"<systemList>\n"
			"	<!-- Here's an example system to get you started. -->\n"
			"	<system>\n"
			"\n"
			"		<!-- A short name, used internally. Traditionally lower-case. -->\n"
			"		<name>nes</name>\n"
			"\n"
			"		<!-- A \"pretty\" name, displayed in menus and such. -->\n"
			"		<fullname>Nintendo Entertainment System</fullname>\n"
			"\n"
			"		<!-- The path to start searching for ROMs in. '~' will be expanded to $HOME on Linux or %HOMEPATH% on Windows. -->\n"
			"		<path>~/roms/nes</path>\n"
			"\n"
			"		<!-- A list of extensions to search for, delimited with any of the whitespace characters (\", \\r\\n\\t\").\n"
			"		You MUST include the period at the start of the extension! It's also case sensitive. -->\n"
			"		<extension>.nes .NES</extension>\n"
			"\n"
			"		<!-- The shell command executed when a game is selected. A few special tags are replaced if found in a command:\n"
			"		%ROM% is replaced by a bash-special-character-escaped absolute path to the ROM.\n"
			"		%BASENAME% is replaced by the \"base\" name of the ROM.  For example, \"/foo/bar.rom\" would have a basename of \"bar\". Useful for MAME.\n"
			"		%ROM_RAW% is the raw, unescaped path to the ROM. -->\n"
			"		<command>retroarch -L ~/cores/libretro-fceumm.so %ROM%</command>\n"
			"\n"
			"		<!-- The platform to use when scraping. You can see the full list of accepted platforms in src/PlatformIds.cpp.\n"
			"		It's case sensitive, but everything is lowercase. This tag is optional.\n"
			"		You can use multiple platforms too, delimited with any of the whitespace characters (\", \\r\\n\\t\"), eg: \"genesis, megadrive\" -->\n"
			"		<platform>nes</platform>\n"
			"\n"
			"		<!-- The theme to load from the current theme set.  See THEMES.md for more information.\n"
			"		This tag is optional. If not set, it will default to the value of <name>. -->\n"
			"		<theme>nes</theme>\n"
			"	</system>\n"
			"</systemList>\n";

	file.close();

	LOG(LogError) << "Example config written!  Go read it at \"" << path << "\"!";
}

void SystemData::deleteSystems()
{
	for(unsigned int i = 0; i < sSystemVector.size(); i++)
	{
		delete sSystemVector.at(i);
	}
	sSystemVector.clear();
}

std::string SystemData::getConfigPath(bool forWrite)
{
	std::string path = Utils::FileSystem::getHomePath() + "/.emulationstation/es_systems.cfg";
	if(forWrite || Utils::FileSystem::exists(path))
		return path;

	return "/etc/emulationstation/es_systems.cfg";
}

bool SystemData::isVisible()
{
   return (getDisplayedGameCount() > 0 ||
           (UIModeController::getInstance()->isUIModeFull() && mIsCollectionSystem) ||
           (mIsCollectionSystem && mName == "favorites"));
}

SystemData* SystemData::getNext() const
{
	std::vector<SystemData*>::const_iterator it = getIterator();

	do {
		it++;
		if (it == sSystemVector.cend())
			it = sSystemVector.cbegin();
	} while (!(*it)->isVisible());

	return *it;
}

SystemData* SystemData::getPrev() const
{
	std::vector<SystemData*>::const_reverse_iterator it = getRevIterator();

	do {
		it++;
		if (it == sSystemVector.crend())
			it = sSystemVector.crbegin();
	} while (!(*it)->isVisible());

	return *it;
}

std::string SystemData::getGamelistPath(bool forWrite) const
{
	std::string filePath;

	filePath = mRootFolder->getPath() + "/gamelist.xml";
	if(Utils::FileSystem::exists(filePath))
		return filePath;

	filePath = Utils::FileSystem::getHomePath() + "/.emulationstation/gamelists/" + mName + "/gamelist.xml";
	if(forWrite)
		Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(filePath));
	if(forWrite || Utils::FileSystem::exists(filePath))
		return filePath;

	return "/etc/emulationstation/gamelists/" + mName + "/gamelist.xml";
}

std::string SystemData::getThemePath() const
{
	std::string localThemePath = mRootFolder->getPath() + "/theme.xml";
	if(Utils::FileSystem::exists(localThemePath))
		return localThemePath;

	localThemePath = ThemeData::getThemeFromCurrentSet(mThemeFolder);

	if (Utils::FileSystem::exists(localThemePath))
		return localThemePath;

	localThemePath = Utils::FileSystem::getParent(Utils::FileSystem::getParent(localThemePath)) + "/theme.xml";
	return localThemePath;
}

bool SystemData::hasGamelist() const
{
	return (Utils::FileSystem::exists(getGamelistPath(false)));
}

unsigned int SystemData::getGameCount() const
{
	return (unsigned int)mRootFolder->getFilesRecursive(GAME).size();
}

SystemData* SystemData::getRandomSystem()
{
	if (sSystemVector.empty()) return NULL;

	if (sSystemVectorShuffled.empty())
	{
		std::copy_if(sSystemVector.begin(), sSystemVector.end(), std::back_inserter(sSystemVectorShuffled),
			[](SystemData *sd){ return sd->isGameSystem(); });

		if (sSystemVectorShuffled.empty()) return NULL;
		std::shuffle(sSystemVectorShuffled.begin(), sSystemVectorShuffled.end(), sURNG);
	}

	SystemData* random_system = sSystemVectorShuffled.back();
	sSystemVectorShuffled.pop_back();
	return random_system;
}

void SystemData::setShuffledCacheDirty()
{
	mGamesShuffled.clear();
}

FileData* SystemData::getRandomGame()
{
	if (mGamesShuffled.empty())
	{
		mGamesShuffled = mRootFolder->getFilesRecursive(GAME, true);
		if (mGamesShuffled.empty()) return NULL;
		std::shuffle(mGamesShuffled.begin(), mGamesShuffled.end(), sURNG);
	}
	FileData* random_game = mGamesShuffled.back();
	mGamesShuffled.pop_back();
	return random_game;
}

unsigned int SystemData::getDisplayedGameCount() const
{
	return (unsigned int)mRootFolder->getFilesRecursive(GAME, true).size();
}

unsigned int SystemData::getFavoriteCount() const
{
	unsigned int fav = 0;

	auto games = mRootFolder->getFilesRecursive(GAME, true);
	for (auto game : games)
	{
		const std::string& favStr = game->metadata.get("favorite");
		if (favStr == "true" || favStr == "1")
			++fav;
	}

	return fav;
}

unsigned int SystemData::getMostPlayedCount() const
{
	unsigned int maxcount = 0;

	auto games = mRootFolder->getFilesRecursive(GAME, true);
	for (auto game : games)
	{
		int pc = game->metadata.getInt("playcount");
		if (pc > (int)maxcount)
			maxcount = (unsigned int)pc;
	}

	return maxcount;
}

std::string SystemData::getMostPlayedName() const
{
	std::string bestName;
	int maxcount = -1;

	auto games = mRootFolder->getFilesRecursive(GAME, true);
	for (auto game : games)
	{
		int pc = game->metadata.getInt("playcount");
		if (pc > maxcount)
		{
			maxcount = pc;

			bestName = game->metadata.get("name");
			if (bestName.empty())
				bestName = Utils::FileSystem::getStem(game->getPath());
		}
	}

	// Sin juegos o nadie jugado → mostramos "N/A"
	if (maxcount <= 0 || bestName.empty())
		return "N/A";

	// Límite de 22 caracteres como acordamos
	return truncateMostPlayedName(bestName, 22);
}

std::string SystemData::getMostPlayedFull() const
{
	std::string name = getMostPlayedName();

	if (name == "N/A")
		return "N/A";

	unsigned int count = getMostPlayedCount();
	if (count == 0)
		return name;

	return name + " (" + std::to_string(count) + ")";
}

// ✅ NUEVO: imagen del más jugado con fallback pulido (boxart → image → thumbnail → titleshot → marquee)
std::string SystemData::getMostPlayedImage() const
{
	FileData* best = nullptr;
	int maxcount = -1;

	auto games = mRootFolder->getFilesRecursive(GAME, true);
	for (auto game : games)
	{
		int pc = game->metadata.getInt("playcount");
		if (pc > maxcount)
		{
			maxcount = pc;
			best = game;
		}
	}

	if (!best || maxcount <= 0)
		return "";

	// Orden pulido (lo que acordamos): boxart → image → thumbnail → (titleshot opcional) → marquee
	std::string raw = best->metadata.get("boxart");
	if (raw.empty()) raw = best->metadata.get("image");
	if (raw.empty()) raw = best->metadata.get("thumbnail");
	if (raw.empty()) raw = best->metadata.get("titleshot"); // opcional, lo dejamos porque ya lo usabas
	if (raw.empty()) raw = best->metadata.get("marquee");
	if (raw.empty())
		return "";

	raw = expandHomeTilde(raw);

	// 1) absoluta y existe
	if (!raw.empty() && raw[0] == '/' && fileExistsAny(raw))
		return raw;

	// 2) relativa -> resolver contra carpeta del gamelist
	{
		std::string base = Utils::FileSystem::getParent(getGamelistPath(false));
		std::string candidate = raw;

		if (candidate.size() >= 2 && candidate[0] == '.' && candidate[1] == '/')
			candidate = candidate.substr(2);

		candidate = Utils::FileSystem::getGenericPath(base + "/" + candidate);
		if (fileExistsAny(candidate))
			return candidate;
	}

	// 3) fallback por filename a tus 2 rutas
	std::string filename = Utils::FileSystem::getFileName(raw);
	if (filename.empty())
		return "";

	// A) ~/.emulationstation/downloaded_images/<system>/
	{
		std::string p = Utils::FileSystem::getHomePath()
			+ "/.emulationstation/downloaded_images/"
			+ mName + "/" + filename;

		p = Utils::FileSystem::getGenericPath(p);
		if (fileExistsAny(p))
			return p;
	}

	// B) ~/RetroPie/roms/<system>/media/screenshots/
	{
		std::string p = Utils::FileSystem::getHomePath()
			+ "/RetroPie/roms/"
			+ mName + "/media/screenshots/" + filename;

		p = Utils::FileSystem::getGenericPath(p);
		if (fileExistsAny(p))
			return p;
	}

	// 4) si no hay extensión, probar comunes
	std::string ext = Utils::FileSystem::getExtension(filename);
	std::string stem = Utils::FileSystem::getStem(filename);

	if (ext.empty() && !stem.empty())
	{
		const char* exts[] = { ".png", ".jpg", ".jpeg", ".webp" };

		for (auto e : exts)
		{
			std::string pA = Utils::FileSystem::getHomePath()
				+ "/.emulationstation/downloaded_images/" + mName + "/" + stem + e;
			pA = Utils::FileSystem::getGenericPath(pA);
			if (fileExistsAny(pA))
				return pA;

			std::string pB = Utils::FileSystem::getHomePath()
				+ "/RetroPie/roms/" + mName + "/media/screenshots/" + stem + e;
			pB = Utils::FileSystem::getGenericPath(pB);
			if (fileExistsAny(pB))
				return pB;
		}
	}

	return "";
}

void SystemData::loadTheme()
{
	mTheme = std::make_shared<ThemeData>();

	std::string path = getThemePath();

	if(!Utils::FileSystem::exists(path)) // no theme available for this platform
		return;

	try
	{
		// build map with system variables for theme to use,
		std::map<std::string, std::string> sysData;
		sysData.insert(std::make_pair("system.name", getName()));
		sysData.insert(std::make_pair("system.theme", getThemeFolder()));
		sysData.insert(std::make_pair("system.fullName", getFullName()));

		// Extras para temas (contadores y "más jugado")
		sysData.insert(std::make_pair("system.gameCount", std::to_string(getGameCount())));
		sysData.insert(std::make_pair("system.displayedGameCount", std::to_string(getDisplayedGameCount())));
		sysData.insert(std::make_pair("system.favoriteCount", std::to_string(getFavoriteCount())));
		sysData.insert(std::make_pair("system.mostPlayedCount", std::to_string(getMostPlayedCount())));
		sysData.insert(std::make_pair("system.mostPlayedName", getMostPlayedName()));
		sysData.insert(std::make_pair("system.mostPlayedFull", getMostPlayedFull()));

		// ✅ NUEVO: ruta de imagen resuelta
		sysData.insert(std::make_pair("system.mostPlayedImage", getMostPlayedImage()));

		mTheme->loadFile(sysData, path);
	}
	catch(ThemeException& e)
	{
		LOG(LogError) << e.what();
		mTheme = std::make_shared<ThemeData>(); // reset to empty
	}
}

void SystemData::writeMetaData() {
	if(Settings::getInstance()->getBool("IgnoreGamelist") || mIsCollectionSystem)
		return;

	//save changed game data back to xml
	updateGamelist(this);
}

void SystemData::onMetaDataSavePoint() {
	if(Settings::getInstance()->getString("SaveGamelistsMode") != "always")
		return;

	writeMetaData();
}
