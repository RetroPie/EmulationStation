#pragma once
#ifndef ES_APP_GUIS_GUI_MENU_H
#define ES_APP_GUIS_GUI_MENU_H

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/TextComponent.h"
#include "HelpStyle.h"
#include "HelpPrompt.h"

class GuiMenu : public GuiComponent
{
public:
	GuiMenu(Window* window);
	virtual ~GuiMenu() {}

	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	HelpStyle getHelpStyle() override;

private:
	MenuComponent mMenu;
	TextComponent mVersion;

	// Bloques del menú principal
	void openScraperSettings();
	void openSoundSettings();
	void openUISettings();
	void openOtherSettings();
	void openConfigInput();
	void openQuitMenu();
	void openCollectionSystemSettings();
	void openScreensaverOptions();

	// NUEVO: opciones de tema internas (GuiThemeOptions)
	void openThemeOptions();

	// Helpers internos
	void addEntry(const char* name, unsigned int color, bool add_arrow, const std::function<void()>& func);
	void addVersionInfo();

	void onSizeChanged() override;
};

#endif // ES_APP_GUIS_GUI_MENU_H
