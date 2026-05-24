#pragma once
#ifndef ES_APP_GUIS_GUI_ROM_SELECTOR_H
#define ES_APP_GUIS_GUI_ROM_SELECTOR_H

#include "guis/GuiSettings.h"
#include "FileData.h"
#include <functional>
#include <memory>
#include <vector>

class ImageComponent;

class GuiRomSelector : public GuiSettings
{
public:
	GuiRomSelector(Window* window, FileData* game, std::function<void()> metadataChangedCallback);
	virtual ~GuiRomSelector();

private:
	struct RomRowWidgets
	{
		std::shared_ptr<ImageComponent> star;
	};

	void buildRows();
	void openMetaEd(unsigned int romIndex);
	void updatePreferredIndicator();

	FileData* mGame;
	std::function<void()> mMetadataChangedCallback;
	bool mAnyEdited;
	std::vector<RomRowWidgets> mRows;
};

#endif // ES_APP_GUIS_GUI_ROM_SELECTOR_H
