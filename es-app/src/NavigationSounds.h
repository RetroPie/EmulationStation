#pragma once
#ifndef ES_APP_NAVIGATION_SOUNDS_H
#define ES_APP_NAVIGATION_SOUNDS_H

#include <memory>
#include <string>
#include <vector>

#include "ThemeData.h"
#include "Sound.h"

// Helper simple para obtener sonidos de navegación por nombre lógico.
// Ejemplos de logicalName: "scroll", "back", "select", "favorite", "launch",
// "systembrowse", "quicksysselect".
class NavigationSounds
{
public:
	static std::shared_ptr<Sound> getFromTheme(
		const std::shared_ptr<ThemeData>& theme,
		const std::string& logicalName)
	{
		if (!theme)
			return nullptr;

		// 1) Nombres candidatos: primero el nombre lógico
		std::vector<std::string> elementNames;
		elementNames.push_back(logicalName);

		// 2) Alias de compatibilidad (para temas existentes)
		if (logicalName == "scroll")
		{
			// nombres usados en algunos temas
			elementNames.push_back("scrollSound");
			elementNames.push_back("scrollSoundList");
			elementNames.push_back("scrollSoundDetailed");
		}
		else if (logicalName == "select")
		{
			elementNames.push_back("selectSound");
		}
		else if (logicalName == "back")
		{
			elementNames.push_back("backSound");
		}

		// 3) Vistas donde buscamos primero
		// "all" → para cuando hagamos <view name="all">
		static const char* viewsToCheck[] = { "all", "system", "gamelist", "menu" };

		for (const char* viewName : viewsToCheck)
		{
			for (const auto& elemName : elementNames)
			{
				const ThemeData::ThemeElement* elem =
					theme->getElement(viewName, elemName, "sound");

				if (elem && elem->has("path"))
				{
					std::string path = elem->get<std::string>("path");
					if (!path.empty())
						return Sound::get(path);
				}
			}
		}

		return nullptr;
	}
};

#endif // ES_APP_NAVIGATION_SOUNDS_H
