#pragma once
#ifndef ES_APP_GUIS_GUI_ROM_META_DATA_ED_H
#define ES_APP_GUIS_GUI_ROM_META_DATA_ED_H

#include "components/ComponentGrid.h"
#include "components/NinePatchComponent.h"
#include "GuiComponent.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class ComponentList;
class TextComponent;
class FileData;

class GuiRomMetaDataEd : public GuiComponent
{
public:
	GuiRomMetaDataEd(Window* window, FileData* game, unsigned int romIndex, std::function<void()> savedCallback);

	bool input(InputConfig* config, Input input) override;
	void onSizeChanged() override;
	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void save();
	void close(bool closeAllWindows);
	bool hasChanges();

	NinePatchComponent mBackground;
	ComponentGrid mGrid;

	std::shared_ptr<TextComponent> mTitle;
	std::shared_ptr<TextComponent> mSubtitle;
	std::shared_ptr<ComponentGrid> mHeaderGrid;
	std::shared_ptr<ComponentList> mList;
	std::shared_ptr<ComponentGrid> mButtons;

	FileData* mGame;
	unsigned int mRomIndex;
	std::function<void()> mSavedCallback;

	// Editors in order: romName, releaseDate, image, video, thumbnail, marquee, regions, languages
	std::vector<std::shared_ptr<GuiComponent>> mEditors;

	// Original values for change detection (same order as mEditors)
	std::vector<std::string> mOriginalValues;
};

#endif // ES_APP_GUIS_GUI_ROM_META_DATA_ED_H
