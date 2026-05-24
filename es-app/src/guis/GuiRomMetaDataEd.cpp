#include "guis/GuiRomMetaDataEd.h"

#include <sstream>
#include "components/ButtonComponent.h"
#include "components/ComponentList.h"
#include "components/DateTimeEditComponent.h"
#include "components/MenuComponent.h"
#include "components/TextComponent.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiTextEditPopup.h"
#include "resources/Font.h"
#include "utils/StringUtil.h"
#include "views/ViewController.h"
#include "FileData.h"
#include "SystemData.h"
#include "Window.h"

// Join a vector of strings with ", " separator
static std::string joinStrings(const std::vector<std::string>& vec)
{
	std::string result;
	for(unsigned int i = 0; i < vec.size(); ++i)
	{
		if(i > 0)
			result += ", ";
		result += vec[i];
	}
	return result;
}

// Split a string on "," and trim each token, skip empties
static std::vector<std::string> splitTrimmed(const std::string& str)
{
	std::vector<std::string> result;
	std::istringstream ss(str);
	std::string token;
	while(std::getline(ss, token, ','))
	{
		token = Utils::String::trim(token);
		if(!token.empty())
			result.push_back(token);
	}
	return result;
}

GuiRomMetaDataEd::GuiRomMetaDataEd(Window* window, FileData* game, unsigned int romIndex, std::function<void()> savedCallback)
	: GuiComponent(window),
	  mBackground(window, ":/frame.png"),
	  mGrid(window, Vector2i(1, 3)),
	  mGame(game),
	  mRomIndex(romIndex),
	  mSavedCallback(savedCallback)
{
	addChild(&mBackground);
	addChild(&mGrid);

	// Header
	mHeaderGrid = std::make_shared<ComponentGrid>(mWindow, Vector2i(1, 5));
	mTitle = std::make_shared<TextComponent>(mWindow, "EDIT ROM METADATA", Font::get(FONT_SIZE_LARGE), 0x555555FF, ALIGN_CENTER);

	FileData* source = mGame->getSourceFileData();
	const std::vector<RomData>& roms = source->getRoms();
	std::string romLabel;
	if(mRomIndex < roms.size())
		romLabel = roms[mRomIndex].romName.empty() ? Utils::FileSystem::getFileName(roms[mRomIndex].path) : roms[mRomIndex].romName;
	else
		romLabel = Utils::FileSystem::getFileName(source->getPath());

	mSubtitle = std::make_shared<TextComponent>(mWindow, "ROM: " + Utils::String::toUpper(romLabel), Font::get(FONT_SIZE_SMALL), 0x777777FF, ALIGN_CENTER);
	mHeaderGrid->setEntry(mTitle, Vector2i(0, 1), false, true);
	mHeaderGrid->setEntry(mSubtitle, Vector2i(0, 3), false, true);
	mGrid.setEntry(mHeaderGrid, Vector2i(0, 0), false, true);

	mList = std::make_shared<ComponentList>(mWindow);
	mGrid.setEntry(mList, Vector2i(0, 1), true, true);

	// Helper lambda to add a text+arrow row backed by GuiTextEditPopup
	auto addTextRow = [&](const std::string& label, const std::string& initialValue, bool multiLine) {
		ComponentListRow row;
		auto lbl = std::make_shared<TextComponent>(mWindow, label, Font::get(FONT_SIZE_SMALL), 0x777777FF);
		row.addElement(lbl, true);

		auto ed = std::make_shared<TextComponent>(mWindow, "", Font::get(FONT_SIZE_SMALL, FONT_PATH_LIGHT), 0x777777FF, ALIGN_RIGHT);
		const float height = lbl->getSize().y() * 0.71f;
		ed->setSize(0, height);
		row.addElement(ed, true);

		auto spacer = std::make_shared<GuiComponent>(mWindow);
		spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
		row.addElement(spacer, false);

		auto bracket = std::make_shared<ImageComponent>(mWindow);
		bracket->setImage(":/arrow.svg");
		bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));
		row.addElement(bracket, false);

		ed->setValue(initialValue);

		auto updateVal = [ed](const std::string& newVal) { ed->setValue(newVal); };
		row.makeAcceptInputHandler([this, label, ed, updateVal, multiLine] {
			mWindow->pushGui(new GuiTextEditPopup(mWindow, label, ed->getValue(), updateVal, multiLine));
		});

		mList->addRow(row);
		mEditors.push_back(ed);
		mOriginalValues.push_back(initialValue);
	};

	// Helper lambda to add a date row
	auto addDateRow = [&](const std::string& label, const std::string& initialValue) {
		ComponentListRow row;
		auto lbl = std::make_shared<TextComponent>(mWindow, label, Font::get(FONT_SIZE_SMALL), 0x777777FF);
		row.addElement(lbl, true);

		auto ed = std::make_shared<DateTimeEditComponent>(mWindow);
		row.addElement(ed, false);

		auto spacer = std::make_shared<GuiComponent>(mWindow);
		spacer->setSize(Renderer::getScreenWidth() * 0.0025f, 0);
		row.addElement(spacer, false);

		// Pass input to the DateTimeEditComponent
		row.input_handler = std::bind(&GuiComponent::input, ed.get(), std::placeholders::_1, std::placeholders::_2);

		ed->setValue(initialValue);

		mList->addRow(row);
		mEditors.push_back(ed);
		mOriginalValues.push_back(initialValue);
	};

	// Populate fields from current ROM data
	const RomData* rom = (mRomIndex < roms.size()) ? &roms[mRomIndex] : nullptr;

	std::string romName     = rom ? rom->romName     : "";
	std::string releaseDate = rom ? rom->releaseDate : "";
	std::string image       = rom ? rom->image       : "";
	std::string video       = rom ? rom->video       : "";
	std::string thumbnail   = rom ? rom->thumbnail   : "";
	std::string marquee     = rom ? rom->marquee     : "";
	std::string regions     = rom ? joinStrings(rom->regions)   : "";
	std::string languages   = rom ? joinStrings(rom->languages) : "";

	addTextRow("ROM NAME",     romName,     false);  // editor index 0
	addDateRow("RELEASE DATE", releaseDate);          // editor index 1
	addTextRow("IMAGE",        image,       false);  // editor index 2
	addTextRow("VIDEO",        video,       false);  // editor index 3
	addTextRow("THUMBNAIL",    thumbnail,   false);  // editor index 4
	addTextRow("MARQUEE",      marquee,     false);  // editor index 5
	addTextRow("REGIONS",      regions,     false);  // editor index 6
	addTextRow("LANGUAGES",    languages,   false);  // editor index 7

	// Buttons: SAVE, CANCEL
	std::vector<std::shared_ptr<ButtonComponent>> buttons;
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, "SAVE",   "save",   [&] { save(); delete this; }));
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, "CANCEL", "cancel", [&] { delete this; }));

	mButtons = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtons, Vector2i(0, 2), true, false);

	// Resize and center
	float width = (float)Math::min(Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.90f));
	setSize(width, Renderer::getScreenHeight() * 0.82f);
	setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
}

void GuiRomMetaDataEd::onSizeChanged()
{
	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
	mGrid.setSize(mSize);

	const float titleHeight    = mTitle->getFont()->getLetterHeight();
	const float subtitleHeight = mSubtitle->getFont()->getLetterHeight();
	const float titleSubtitleSpacing = mSize.y() * 0.03f;

	mGrid.setRowHeightPerc(0, (titleHeight + titleSubtitleSpacing + subtitleHeight + TITLE_VERT_PADDING) / mSize.y());
	mGrid.setRowHeightPerc(2, mButtons->getSize().y() / mSize.y());

	mHeaderGrid->setRowHeightPerc(1, titleHeight    / mHeaderGrid->getSize().y());
	mHeaderGrid->setRowHeightPerc(2, titleSubtitleSpacing / mHeaderGrid->getSize().y());
	mHeaderGrid->setRowHeightPerc(3, subtitleHeight / mHeaderGrid->getSize().y());
}

void GuiRomMetaDataEd::save()
{
	FileData* source = mGame->getSourceFileData();
	std::vector<RomData>& roms = source->getRomsMutable();
	if(mRomIndex >= roms.size())
		return;

	RomData& rom = roms[mRomIndex];

	// editor indices match the order in the constructor
	rom.romName     = mEditors[0]->getValue();
	rom.releaseDate = mEditors[1]->getValue();
	rom.image       = mEditors[2]->getValue();
	rom.video       = mEditors[3]->getValue();
	rom.thumbnail   = mEditors[4]->getValue();
	rom.marquee     = mEditors[5]->getValue();
	rom.regions     = splitTrimmed(mEditors[6]->getValue());
	rom.languages   = splitTrimmed(mEditors[7]->getValue());

	// Mark dirty so the gamelist writer picks up the change
	source->metadata.set("name", source->metadata.get("name"));
	source->getSystem()->onMetaDataSavePoint();

	ViewController::get()->onFileChanged(source, FILE_METADATA_CHANGED);

	if(mSavedCallback)
		mSavedCallback();
}

void GuiRomMetaDataEd::close(bool closeAllWindows)
{
	bool dirty = hasChanges();

	std::function<void()> closeFunc;
	if(!closeAllWindows)
	{
		closeFunc = [this] { delete this; };
	}
	else
	{
		Window* window = mWindow;
		closeFunc = [window, this] {
			while(window->peekGui() != ViewController::get())
				delete window->peekGui();
		};
	}

	if(dirty)
	{
		mWindow->pushGui(new GuiMsgBox(mWindow,
			"SAVE CHANGES?",
			"YES", [this, closeFunc] { save(); closeFunc(); },
			"NO", closeFunc
		));
	}
	else
	{
		closeFunc();
	}
}

bool GuiRomMetaDataEd::hasChanges()
{
	for(unsigned int i = 0; i < mEditors.size() && i < mOriginalValues.size(); ++i)
	{
		if(mEditors[i]->getValue() != mOriginalValues[i])
			return true;
	}
	return false;
}

bool GuiRomMetaDataEd::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	const bool isStart = config->isMappedTo("start", input);
	if(input.value != 0 && (config->isMappedTo("b", input) || isStart))
	{
		close(isStart);
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiRomMetaDataEd::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mGrid.getHelpPrompts();
	prompts.push_back(HelpPrompt("b", "back"));
	prompts.push_back(HelpPrompt("start", "close"));
	return prompts;
}
