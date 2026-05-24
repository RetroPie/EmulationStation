#include "guis/GuiRomSelectionMenu.h"

#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "views/gamelist/IGameListView.h"
#include "views/ViewController.h"
#include "FileData.h"
#include "SystemData.h"
#include "Window.h"

// Local helper: shows "a: launch, y: preferred" in the help bar for each ROM row
namespace
{
	class RomPromptTextComponent : public TextComponent
	{
	public:
		RomPromptTextComponent(Window* window, const std::string& text, const std::shared_ptr<Font>& font, unsigned int color)
			: TextComponent(window, text, font, color)
		{
		}

		std::vector<HelpPrompt> getHelpPrompts() override
		{
			std::vector<HelpPrompt> prompts;
			prompts.push_back(HelpPrompt("a", "launch"));
			prompts.push_back(HelpPrompt("y", "preferred"));
			return prompts;
		}
	};
}

GuiRomSelectionMenu::GuiRomSelectionMenu(Window* window, FileData* game)
	: GuiSettings(window, "ROMS"), mGame(game), mPreferredChanged(false)
{
	buildRows();
}

GuiRomSelectionMenu::~GuiRomSelectionMenu()
{
	if(!mPreferredChanged || mGame == nullptr)
		return;

	// Preferred ROM changes alter effective media/launch selection, so notify both
	// the visible entry and canonical source entry to refresh info panels immediately.
	ViewController::get()->onFileChanged(mGame, FILE_METADATA_CHANGED);
	FileData* source = mGame->getSourceFileData();
	if(source != mGame)
		ViewController::get()->onFileChanged(source, FILE_METADATA_CHANGED);
}

void GuiRomSelectionMenu::buildRows()
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
		auto label = std::make_shared<RomPromptTextComponent>(mWindow, romName, Font::get(FONT_SIZE_MEDIUM), 0x777777FF);

		row.addElement(star, false, false);
		row.addElement(label, true);
		row.input_handler = [this, i](InputConfig* config, Input input) -> bool {
			if(input.value == 0)
				return false;

			if(config->isMappedTo("a", input))
			{
				launchRom(i);
				return true;
			}

			if(config->isMappedTo("y", input))
			{
				setPreferredRom(i);
				return true;
			}

			return false;
		};

		addRow(row);

		RomRowWidgets widgets;
		widgets.star = star;
		widgets.name = label;
		mRows.push_back(widgets);
	}

	updatePreferredIndicator();
}

void GuiRomSelectionMenu::setPreferredRom(unsigned int index)
{
	if(mGame == nullptr)
		return;

	FileData* source = mGame->getSourceFileData();
	std::vector<RomData>& roms = source->getRomsMutable();
	if(index >= roms.size())
		return;

	for(unsigned int i = 0; i < roms.size(); ++i)
		roms[i].preferred = (i == index);

	// Mark metadata dirty so onMetaDataSavePoint() persists updated ROM preference.
	source->metadata.set("name", source->metadata.get("name"));
	source->getSystem()->onMetaDataSavePoint();

	if(mGame != source)
		mGame->refreshMetadata();

	mPreferredChanged = true;
	updatePreferredIndicator();
}

void GuiRomSelectionMenu::launchRom(unsigned int index)
{
	if(mGame == nullptr)
		return;

	FileData* selectedEntry = mGame;
	FileData* source = selectedEntry->getSourceFileData();
	const std::vector<RomData>& roms = source->getRoms();
	if(index >= roms.size() || roms[index].path.empty())
		return;

	const std::string launchPath = roms[index].path;
	std::shared_ptr<IGameListView> currentGameList = ViewController::get()->getGameListView(selectedEntry->getSystem());
	if(currentGameList)
		currentGameList->onHide();

	// Keep this popup alive so returning from gameplay lands back on the
	// same ROM selection screen and highlighted row.
	source->launchGame(mWindow, launchPath);
	ViewController::get()->onFileChanged(source, FILE_METADATA_CHANGED);
	if(selectedEntry != source)
		ViewController::get()->onFileChanged(selectedEntry, FILE_METADATA_CHANGED);

	if(currentGameList)
	{
		currentGameList->setCursor(selectedEntry, true);
		currentGameList->onShow();
	}
}

void GuiRomSelectionMenu::updatePreferredIndicator()
{
	if(mGame == nullptr)
		return;

	const std::vector<RomData>& roms = mGame->getRoms();
	for(unsigned int i = 0; i < roms.size() && i < mRows.size(); ++i)
		mRows[i].star->setVisible(roms[i].preferred);
}
