#pragma once
#ifndef ES_APP_GUIS_GUI_ROM_SELECTION_MENU_H
#define ES_APP_GUIS_GUI_ROM_SELECTION_MENU_H

#include "guis/GuiSettings.h"
#include "FileData.h"
#include <memory>
#include <vector>

class ImageComponent;
class TextComponent;

class GuiRomSelectionMenu : public GuiSettings
{
public:
	GuiRomSelectionMenu(Window* window, FileData* game);
	~GuiRomSelectionMenu();

private:
	struct RomRowWidgets
	{
		std::shared_ptr<ImageComponent> star;
		std::shared_ptr<TextComponent> name;
	};

	void buildRows();
	void setPreferredRom(unsigned int index);
	void launchRom(unsigned int index);
	void updatePreferredIndicator();

	FileData* mGame;
	bool mPreferredChanged;
	std::vector<RomRowWidgets> mRows;
};

#endif // ES_APP_GUIS_GUI_ROM_SELECTION_MENU_H
