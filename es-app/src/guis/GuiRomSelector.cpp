#include "guis/GuiRomSelector.h"

#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "guis/GuiRomMetaDataEd.h"
#include "resources/Font.h"
#include "FileData.h"
#include "Window.h"

GuiRomSelector::GuiRomSelector(Window* window, FileData* game, std::function<void()> metadataChangedCallback)
	: GuiSettings(window, "SELECT ROM"), mGame(game), mMetadataChangedCallback(metadataChangedCallback), mAnyEdited(false)
{
	buildRows();
}

GuiRomSelector::~GuiRomSelector()
{
	if(mAnyEdited && mMetadataChangedCallback)
		mMetadataChangedCallback();
}

void GuiRomSelector::buildRows()
{
	if(mGame == nullptr)
		return;

	const std::vector<RomData>& roms = mGame->getRoms();
	for(unsigned int i = 0; i < roms.size(); ++i)
	{
		ComponentListRow row;

		auto star = std::make_shared<ImageComponent>(mWindow);
		const float starSize = Font::get(FONT_SIZE_MEDIUM)->getLetterHeight();
		star->setImage(":/cartridge.svg");
		star->setResize(starSize, starSize);
		star->setVisible(roms[i].preferred);

		std::string romName = roms[i].romName.empty() ? mGame->getName() : roms[i].romName;
		auto label = std::make_shared<TextComponent>(mWindow, romName, Font::get(FONT_SIZE_MEDIUM), 0x777777FF);

		auto bracket = std::make_shared<ImageComponent>(mWindow);
		bracket->setImage(":/arrow.svg");
		bracket->setResize(Vector2f(0, Font::get(FONT_SIZE_MEDIUM)->getLetterHeight()));

		row.addElement(star, false, false);
		row.addElement(label, true);
		row.addElement(bracket, false);

		row.makeAcceptInputHandler([this, i] { openMetaEd(i); });

		addRow(row);

		RomRowWidgets widgets;
		widgets.star = star;
		mRows.push_back(widgets);
	}

	updatePreferredIndicator();
}

void GuiRomSelector::openMetaEd(unsigned int romIndex)
{
	mWindow->pushGui(new GuiRomMetaDataEd(mWindow, mGame, romIndex, [this] {
		mAnyEdited = true;
		updatePreferredIndicator();
	}));
}

void GuiRomSelector::updatePreferredIndicator()
{
	if(mGame == nullptr)
		return;

	const std::vector<RomData>& roms = mGame->getRoms();
	for(unsigned int i = 0; i < roms.size() && i < mRows.size(); ++i)
		mRows[i].star->setVisible(roms[i].preferred);
}
