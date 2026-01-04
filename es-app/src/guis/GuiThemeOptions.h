// es-app/src/guis/GuiThemeOptions.h
#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"

class GuiThemeOptions : public GuiComponent
{
public:
	explicit GuiThemeOptions(Window* window);

	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	HelpStyle getHelpStyle() override;

private:
	MenuComponent mMenu;
};
