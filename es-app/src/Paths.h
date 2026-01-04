#pragma once

#include <string>
#include "utils/FileSystemUtil.h"

namespace Paths
{
	// ~/.emulationstation
	inline std::string getUserEmulationStationPath()
	{
		return Utils::FileSystem::getHomePath() + "/.emulationstation";
	}

	// ~/.emulationstation/music
	inline std::string getUserMusicPath()
	{
		return getUserEmulationStationPath() + "/music";
	}

	// ~/RetroPie/music
	inline std::string getMusicPath()
	{
		return Utils::FileSystem::getHomePath() + "/RetroPie/music";
	}
}
