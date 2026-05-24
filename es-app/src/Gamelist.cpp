#include "Gamelist.h"

#include <chrono>

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "FileData.h"
#include "FileFilterIndex.h"
#include "Log.h"
#include "Settings.h"
#include "SystemData.h"
#include <pugixml.hpp>
#include <unordered_map>

namespace
{
	// Parses either nested (e.g. <languages><language>en</language></languages>)
	// or repeated flat tags (e.g. <language>en</language><language>fr</language>)
	// into a normalized vector.
	std::vector<std::string> parseRomMultiValue(const pugi::xml_node& romNode, const char* singularTag, const char* containerTag)
	{
		std::vector<std::string> values;

		pugi::xml_node container = romNode.child(containerTag);
		if(container)
		{
			for(pugi::xml_node node = container.child(singularTag); node; node = node.next_sibling(singularTag))
			{
				const std::string value = Utils::String::trim(node.text().get());
				if(!value.empty())
					values.push_back(value);
			}
		}

		if(values.empty())
		{
			for(pugi::xml_node node = romNode.child(singularTag); node; node = node.next_sibling(singularTag))
			{
				const std::string value = Utils::String::trim(node.text().get());
				if(!value.empty())
					values.push_back(value);
			}
		}

		return values;
	}

	bool parsePreferredRomAttribute(const pugi::xml_node& romNode)
	{
		pugi::xml_attribute preferredAttr = romNode.attribute("preferred");
		if(!preferredAttr)
			return false;

		std::string val = Utils::String::toLower(Utils::String::trim(preferredAttr.as_string()));
		if(val == "true")
			return true;
		if(val == "false")
			return false;

		LOG(LogWarning) << "Invalid preferred value '" << preferredAttr.as_string() << "' on <rom>; expected 'true' or 'false'. Treating as false.";
		return false;
	}

	RomData parseRomNode(const pugi::xml_node& romNode, const std::string& relativeTo)
	{
		RomData rom;
		rom.path = Utils::FileSystem::resolveRelativePath(romNode.child("path").text().get(), relativeTo, false, true);
		rom.romName = romNode.child("romname").text().get();
		rom.regions = parseRomMultiValue(romNode, "region", "regions");
		rom.languages = parseRomMultiValue(romNode, "language", "languages");
		rom.releaseDate = romNode.child("releasedate").text().get();
		rom.image = Utils::FileSystem::resolveRelativePath(romNode.child("image").text().get(), relativeTo, true, true);
		rom.video = Utils::FileSystem::resolveRelativePath(romNode.child("video").text().get(), relativeTo, true, true);
		rom.thumbnail = Utils::FileSystem::resolveRelativePath(romNode.child("thumbnail").text().get(), relativeTo, true, true);
		rom.marquee = Utils::FileSystem::resolveRelativePath(romNode.child("marquee").text().get(), relativeTo, true, true);
		rom.preferred = parsePreferredRomAttribute(romNode);
		return rom;
	}

	void appendMultiValueNode(pugi::xml_node& parent, const char* containerTag, const char* childTag, const std::vector<std::string>& values)
	{
		if(values.empty())
			return;

		pugi::xml_node container = parent.append_child(containerTag);
		for(auto it = values.cbegin(); it != values.cend(); ++it)
			container.append_child(childTag).text().set(it->c_str());
	}

	void appendOptionalTextNode(pugi::xml_node& parent, const char* tag, const std::string& value)
	{
		if(!value.empty())
			parent.append_child(tag).text().set(value.c_str());
	}

	void appendOptionalPathNode(pugi::xml_node& parent, const char* tag, const std::string& value, const std::string& relativeTo)
	{
		if(value.empty())
			return;

		std::string relValue = Utils::FileSystem::createRelativePath(value, relativeTo, true, true);
		parent.append_child(tag).text().set(relValue.c_str());
	}

	std::string getGamePathForNode(const pugi::xml_node& gameNode, const std::string& relativeTo)
	{
		// New format can omit top-level <path>. In that case we choose the preferred
		// ROM path as the canonical lookup key for findOrCreateFile.
		std::string path = gameNode.child("path").text().get();
		if(!path.empty())
			return Utils::FileSystem::resolveRelativePath(path, relativeTo, false, true);

		pugi::xml_node romsNode = gameNode.child("roms");
		if(!romsNode)
			return "";

		pugi::xml_node firstRom;
		for(pugi::xml_node romNode = romsNode.child("rom"); romNode; romNode = romNode.next_sibling("rom"))
		{
			if(!firstRom)
				firstRom = romNode;

			if(parsePreferredRomAttribute(romNode))
				return Utils::FileSystem::resolveRelativePath(romNode.child("path").text().get(), relativeTo, false, true);
		}

		if(firstRom)
			return Utils::FileSystem::resolveRelativePath(firstRom.child("path").text().get(), relativeTo, false, true);

		return "";
	}
}

FileData* findOrCreateFile(SystemData* system, const std::string& path, FileType type)
{
	FileData* root = system->getRootFolder();
	bool contains = false;
	const std::string systemPath = root->getPath();

	// first, verify that path is within the system's root folder
	std::string relative = Utils::FileSystem::removeCommonPath(path, systemPath, contains, true);
	if(!contains)
	{
		LOG(LogError) << "File path \"" << path << "\" is outside system path \"" << system->getStartPath() << "\"";
		return NULL;
	}

	Utils::FileSystem::stringList pathList = Utils::FileSystem::getPathList(relative);

	auto path_it = pathList.begin();
	FileData* treeNode = root;
	bool found = false;

	// iterate over all subpaths below the provided path
	while(path_it != pathList.end())
	{
		const std::unordered_map<std::string, FileData*>& children = treeNode->getChildrenByFilename();

		std::string pathSegment = *path_it;
		auto candidate = children.find(pathSegment);
		found = candidate != children.cend();
		if (found) {
			treeNode = candidate->second;
		}

		// this is the end
		if(path_it == --pathList.end())
		{
			if(found)
				return treeNode;

			if(type == FOLDER)
			{
				LOG(LogWarning) << "gameList: folder doesn't already exist, won't create";
				return NULL;
			}

			FileData* file = new FileData(type, path, system->getSystemEnvData(), system);

			// skipping arcade assets from gamelist and add only to filesystem
			// (fs) folders, i.e. entriess in gamelist with <folder/> and not to
			// fs-folders which are marked as <game/> in gamelist. NB:
			// treeNode's type (=parent) is determined by the element in the
			// gamelist and not by the fs-type.
			if(!file->isArcadeAsset() && treeNode->getType() == FOLDER)
			{
				treeNode->addChild(file);
			}
			return file;
		}

		if(!found)
		{
			// don't create folders unless it's leading up to a game
			// if type is a folder it's gonna be empty, so don't bother
			if(type == FOLDER)
			{
				std::string absFolder = Utils::FileSystem::getAbsolutePath(pathSegment, systemPath);
				LOG(LogWarning) << "gameList: folder " << absFolder << " absent on fs, no FileData object created. Do remove leftover in gamelist.xml to remediate this warning.";
				return NULL;
			}
			// discard constellations like scummvm/game.svm/game.svm as
			// scummvm/game.svm/ is a GAME and not a FOLDER
			if (treeNode->getType() == GAME)
			{
				std::string absFolder = Utils::FileSystem::getAbsolutePath(pathSegment, systemPath);
				LOG(LogWarning) << "gameList: trying to add game '" << absFolder << "' to a parent <game/> entry is invalid, no FileData object created. Do remove nested <game/> in gamelist.xml to remediate this warning.";
				return NULL;
			}
			// create folder filedata object
			std::string absPath = Utils::FileSystem::resolveRelativePath(treeNode->getPath() + "/" + pathSegment, systemPath, false, true);
			FileData* folder = new FileData(FOLDER, absPath, system->getSystemEnvData(), system);
			LOG(LogDebug) << "folder not found as FileData, adding: " << folder->getPath();

			treeNode->addChild(folder);
			treeNode = folder;
		}

		path_it++;
	}

	return NULL;
}

std::unordered_map<std::string, FileData*> buildGamePathIndex(SystemData* system)
{
	std::unordered_map<std::string, FileData*> index;
	FileData* rootFolder = system->getRootFolder();
	if(rootFolder == NULL)
		return index;

	const std::vector<FileData*> games = rootFolder->getFilesRecursive(GAME);
	for(auto it = games.cbegin(); it != games.cend(); ++it)
		index[(*it)->getPath()] = *it;

	return index;
}

void parseGamelist(SystemData* system)
{
	const auto parseStartTs = std::chrono::steady_clock::now();

	bool trustGamelist = Settings::getInstance()->getBool("ParseGamelistOnly");
	std::string xmlpath = system->getGamelistPath(false);
	const std::vector<std::string> allowedExtensions = system->getExtensions();

	if(!Utils::FileSystem::exists(xmlpath))
		return;

	LOG(LogInfo) << "Parsing XML file \"" << xmlpath << "\"...";

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(xmlpath.c_str());

	if(!result)
	{
		LOG(LogError) << "Error parsing XML file \"" << xmlpath << "\"!\n	" << result.description();
		return;
	}

	pugi::xml_node root = doc.child("gameList");
	if(!root)
	{
		LOG(LogError) << "Could not find <gameList> node in gamelist \"" << xmlpath << "\"!";
		return;
	}

	std::string relativeTo = system->getStartPath();
	const auto indexStartTs = std::chrono::steady_clock::now();
	std::unordered_map<std::string, FileData*> gamePathIndex = buildGamePathIndex(system);
	const auto indexEndTs = std::chrono::steady_clock::now();

	size_t hierarchicalGameCount = 0;
	size_t romVariantCount = 0;
	size_t duplicateRemovedCount = 0;
	const auto parseLoopStartTs = std::chrono::steady_clock::now();

	const char* tagList[2] = { "game", "folder" };
	FileType typeList[2] = { GAME, FOLDER };
	for(int i = 0; i < 2; i++)
	{
		const char* tag = tagList[i];
		FileType type = typeList[i];
		for(pugi::xml_node fileNode = root.child(tag); fileNode; fileNode = fileNode.next_sibling(tag))
		{
			std::string path = (type == GAME) ? getGamePathForNode(fileNode, relativeTo) : Utils::FileSystem::resolveRelativePath(fileNode.child("path").text().get(), relativeTo, false, true);

			if(path.empty())
			{
				LOG(LogWarning) << "Missing path for <" << tag << "> entry, skipping.";
				continue;
			}

			if(!trustGamelist && !Utils::FileSystem::exists(path))
			{
				LOG(LogWarning) << "File \"" << path << "\" does not exist! Ignoring.";
				continue;
			}

			// Check whether the file's extension is allowed in the system
			if (i == 0 /*game*/ && std::find(allowedExtensions.cbegin(), allowedExtensions.cend(), Utils::FileSystem::getExtension(path)) == allowedExtensions.cend())
			{
				LOG(LogDebug) << "file " << path << " found in gamelist, but has unregistered extension";
				continue;
			}

			FileData* file = findOrCreateFile(system, path, type);
			if(!file)
			{
				LOG(LogError) << "Error finding/creating FileData for \"" << path << "\", skipping.";
				continue;
			}
			else if(!file->isArcadeAsset())
			{
				if(type == GAME)
					gamePathIndex[file->getPath()] = file;

				std::string defaultName = file->metadata.get("name");
				file->metadata = MetaDataList::createFromXML(file->getType() == GAME ? GAME_METADATA : FOLDER_METADATA, fileNode, relativeTo);
				if(type == GAME)
				{
					// Rebuild ROM variants from XML every parse so in-memory state reflects
					// both old flat and new hierarchical formats consistently.
					std::vector<RomData>& roms = file->getRomsMutable();
					roms.clear();

					pugi::xml_node romsNode = fileNode.child("roms");
					if(romsNode)
					{
						hierarchicalGameCount++;
						for(pugi::xml_node romNode = romsNode.child("rom"); romNode; romNode = romNode.next_sibling("rom"))
						{
							RomData rom = parseRomNode(romNode, relativeTo);
							if(!rom.path.empty())
							{
								roms.push_back(rom);
								romVariantCount++;
							}
							
						}
					}

					if(roms.empty())
					{
						// Backward-compat: old flat <game> entries become one game + one rom.
						RomData rom;
						rom.path = path;
						rom.romName = fileNode.child("romname").text().as_string(fileNode.child("name").text().get());
						rom.regions = parseRomMultiValue(fileNode, "region", "regions");
						rom.languages = parseRomMultiValue(fileNode, "language", "languages");
						rom.releaseDate = fileNode.child("releasedate").text().get();
						rom.image = Utils::FileSystem::resolveRelativePath(fileNode.child("image").text().get(), relativeTo, true, true);
						rom.video = Utils::FileSystem::resolveRelativePath(fileNode.child("video").text().get(), relativeTo, true, true);
						rom.thumbnail = Utils::FileSystem::resolveRelativePath(fileNode.child("thumbnail").text().get(), relativeTo, true, true);
						rom.marquee = Utils::FileSystem::resolveRelativePath(fileNode.child("marquee").text().get(), relativeTo, true, true);
						rom.preferred = true;
						roms.push_back(rom);
						romVariantCount++;
					}

					bool hasPreferred = false;
					for(auto rit = roms.cbegin(); rit != roms.cend(); ++rit)
					{
						if(rit->preferred)
						{
							hasPreferred = true;
							break;
						}
					}
					if(!hasPreferred)
						// Deterministic fallback when preferred is absent in source XML.
						roms.front().preferred = true;

					// Preserve legacy behavior where releasedate was expected on game-level metadata.
					if((file->metadata.get("releasedate").empty() || file->metadata.get("releasedate") == "not-a-date-time") && !roms.front().releaseDate.empty())
						file->metadata.set("releasedate", roms.front().releaseDate);

					// Keep one canonical FileData entry per hierarchical game. Any other
					// ROM paths in this same game node are variants and should not remain
					// as standalone top-level entries discovered from filesystem scanning.
					for(auto rit = roms.cbegin(); rit != roms.cend(); ++rit)
					{
						if(rit->path.empty() || rit->path == file->getPath())
							continue;

						auto duplicateIt = gamePathIndex.find(rit->path);
						FileData* duplicate = (duplicateIt != gamePathIndex.cend()) ? duplicateIt->second : NULL;
						if(duplicate != NULL && duplicate->getType() == GAME)
						{
							gamePathIndex.erase(rit->path);
							delete duplicate;
							duplicateRemovedCount++;
						}
					}
				}

				//make sure name gets set if one didn't exist
				if(file->metadata.get("name").empty())
					file->metadata.set("name", defaultName);

				file->metadata.resetChangedFlag();
			}
		}
	}

	const auto parseLoopEndTs = std::chrono::steady_clock::now();
	const auto parseEndTs = std::chrono::steady_clock::now();
	LOG(LogInfo) << "Parsed gamelist performance for system '" << system->getName() << "': "
		<< "index=" << std::chrono::duration_cast<std::chrono::milliseconds>(indexEndTs - indexStartTs).count() << "ms, "
		<< "parse-loop=" << std::chrono::duration_cast<std::chrono::milliseconds>(parseLoopEndTs - parseLoopStartTs).count() << "ms, "
		<< "total=" << std::chrono::duration_cast<std::chrono::milliseconds>(parseEndTs - parseStartTs).count() << "ms, "
		<< "hierarchical-games=" << hierarchicalGameCount << ", "
		<< "rom-variants=" << romVariantCount << ", "
		<< "duplicates-removed=" << duplicateRemovedCount;
}

void addFileDataNode(pugi::xml_node& parent, const FileData* file, const char* tag, SystemData* system)
{
	if(file->getType() == GAME)
	{
		pugi::xml_node newNode = parent.append_child(tag);

		file->metadata.appendToXML(newNode, true, system->getStartPath());
		// New hierarchical format stores launch paths and release dates per ROM.
		newNode.remove_child("path");
		newNode.remove_child("releasedate");

		pugi::xml_node romsNode = newNode.append_child("roms");
		for(auto it = file->getRoms().cbegin(); it != file->getRoms().cend(); ++it)
		{
			pugi::xml_node romNode = romsNode.append_child("rom");
			if(it->preferred)
				romNode.append_attribute("preferred") = "true";

			std::string relPath = Utils::FileSystem::createRelativePath(it->path, system->getStartPath(), false, true);
			romNode.append_child("path").text().set(relPath.c_str());

			appendOptionalTextNode(romNode, "romname", it->romName);
			appendMultiValueNode(romNode, "regions", "region", it->regions);
			appendMultiValueNode(romNode, "languages", "language", it->languages);
			appendOptionalTextNode(romNode, "releasedate", it->releaseDate);
			appendOptionalPathNode(romNode, "image", it->image, system->getStartPath());
			appendOptionalPathNode(romNode, "video", it->video, system->getStartPath());
			appendOptionalPathNode(romNode, "thumbnail", it->thumbnail, system->getStartPath());
			appendOptionalPathNode(romNode, "marquee", it->marquee, system->getStartPath());
		}
		return;
	}

	//create game and add to parent node
	pugi::xml_node newNode = parent.append_child(tag);

	//write metadata
	file->metadata.appendToXML(newNode, true, system->getStartPath());

	if(newNode.children().begin() == newNode.child("name") //first element is name
		&& ++newNode.children().begin() == newNode.children().end() //theres only one element
		&& newNode.child("name").text().get() == file->getDisplayName()) //the name is the default
	{
		//if the only info is the default name, don't bother with this node
		//delete it and ultimately do nothing
		parent.remove_child(newNode);
	}else{
		//there's something useful in there so we'll keep the node, add the path

		// try and make the path relative if we can so things still work if we change the rom folder location in the future
		std::string relPath = Utils::FileSystem::createRelativePath(file->getPath(), system->getStartPath(), false, true);
		newNode.prepend_child("path").text().set(relPath.c_str());
	}
}

void updateGamelist(SystemData* system)
{
	//We do this by reading the XML again, adding changes and then writing it back,
	//because there might be information missing in our systemdata which would then miss in the new XML.
	//We have the complete information for every game though, so we can simply remove a game
	//we already have in the system from the XML, and then add it back from its GameData information...

	if(Settings::getInstance()->getBool("IgnoreGamelist"))
		return;

	pugi::xml_document doc;
	pugi::xml_node root;
	std::string xmlReadPath = system->getGamelistPath(false);

	std::string relativeTo = system->getStartPath();

	if(Utils::FileSystem::exists(xmlReadPath))
	{
		//parse an existing file first
		pugi::xml_parse_result result = doc.load_file(xmlReadPath.c_str());

		if(!result)
		{
			LOG(LogError) << "Error parsing XML file \"" << xmlReadPath << "\"!\n	" << result.description();
			return;
		}

		root = doc.child("gameList");
		if(!root)
		{
			LOG(LogError) << "Could not find <gameList> node in gamelist \"" << xmlReadPath << "\"!";
			return;
		}
	}else{
		//set up an empty gamelist to append to
		root = doc.append_child("gameList");
	}

	std::vector<FileData*> changedGames;
	std::vector<FileData*> changedFolders;

	//now we have all the information from the XML. now iterate through all our games and add information from there
	FileData* rootFolder = system->getRootFolder();
	if (rootFolder != nullptr)
	{
		int numUpdated = 0;

		std::vector<FileData*> files = rootFolder->getFilesRecursive(GAME | FOLDER);

		// Stage 1: iterate through all files in memory, checking for changes
		for(std::vector<FileData*>::const_iterator fit = files.cbegin(); fit != files.cend(); ++fit)
		{

			// do not touch if it wasn't changed anyway
			if (!(*fit)->metadata.wasChanged())
				continue;

			// adding item to changed list
			if ((*fit)->getType() == GAME)
			{
				changedGames.push_back((*fit));
			}
			else
			{
				changedFolders.push_back((*fit));
			}
		}


		// Stage 2: iterate XML if needed, to remove and add changed items
		const char* tagList[2] = { "game", "folder" };
		FileType typeList[2] = { GAME, FOLDER };
		std::vector<FileData*> changedList[2] = { changedGames, changedFolders };

		for(int i = 0; i < 2; i++)
		{
			const char* tag = tagList[i];
			FileType nodeType = typeList[i];
			std::vector<FileData*> changes = changedList[i];

			// check for changed items of this type
			if (changes.size() > 0) {
				// check if the item already exists in the XML
				// if it does, remove all corresponding items before adding
				for(pugi::xml_node fileNode = root.child(tag); fileNode; )
				{
					// we need this as we were deleting the iterator and things would become inconsistent
					pugi::xml_node nextNode = fileNode.next_sibling(tag);

					std::string xmlpath;
					if(nodeType == GAME)
						xmlpath = getGamePathForNode(fileNode, relativeTo);
					else
						xmlpath = Utils::FileSystem::resolveRelativePath(fileNode.child("path").text().get(), relativeTo, false, true);

					if(xmlpath.empty())
					{
						fileNode = nextNode;
						continue;
					}

					for(std::vector<FileData*>::const_iterator cfit = changes.cbegin(); cfit != changes.cend(); ++cfit)
					{
						if(xmlpath == (*cfit)->getPath())
						{
							// found it
							root.remove_child(fileNode);
							break;
						}
					}
					fileNode = nextNode;

				}

				// add items to XML
				for(std::vector<FileData*>::const_iterator cfit = changes.cbegin(); cfit != changes.cend(); ++cfit)
				{
					// it was either removed or never existed to begin with; either way, we can add it now
					addFileDataNode(root, *cfit, tag, system);
					++numUpdated;
				}
			}
		}

		// now write the file

		if (numUpdated > 0) {
			const auto startTs = std::chrono::system_clock::now();

			//make sure the folders leading up to this path exist (or the write will fail)
			std::string xmlWritePath(system->getGamelistPath(true));
			Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(xmlWritePath));

			LOG(LogInfo) << "Added/Updated " << numUpdated << " entities in '" << xmlReadPath << "'";

			if (!doc.save_file(xmlWritePath.c_str())) {
				LOG(LogError) << "Error saving gamelist.xml to \"" << xmlWritePath << "\" (for system " << system->getName() << ")!";
			}

			const auto endTs = std::chrono::system_clock::now();
			LOG(LogInfo) << "Saved gamelist.xml for system \"" << system->getName() << "\" in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTs - startTs).count() << " ms";
		}
	}else{
		LOG(LogError) << "Found no root folder for system \"" << system->getName() << "\"!";
	}
}
