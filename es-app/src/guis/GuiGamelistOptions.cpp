#include "GuiGamelistOptions.h"

#include "guis/GuiGamelistFilter.h"
#include "scrapers/Scraper.h"
#include "views/gamelist/IGameListView.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "FileFilterIndex.h"
#include "FileSorts.h"
#include "GuiMetaDataEd.h"
#include "SystemData.h"
#include "components/TextListComponent.h"
#include "../LocaleES.h"   // 🔹 Sistema de traducción

GuiGamelistOptions::GuiGamelistOptions(Window* window, SystemData* system)
	: GuiComponent(window)
	, mMenu(window, es_translate("OPTIONS").c_str(), Font::get(FONT_SIZE_LARGE)) // 🔹 Título traducible
	, mSystem(system)
	, mFromPlaceholder(false)
	, mFiltersChanged(false)
	, mJumpToSelected(false)
	, mMetadataChanged(false)
{
	addChild(&mMenu);

	// check it's not a placeholder folder - if it is, only show "Filter Options"
	FileData* file = getGamelist()->getCursor();
	mFromPlaceholder = file->isPlaceHolder();
	ComponentListRow row;

	if (!mFromPlaceholder)
	{
		std::string currentSort = mSystem->getRootFolder()->getSortDescription();
		std::string reqSort = FileSorts::SortTypes.at(0).description;

		// "jump to letter" menuitem only available (and correct jumping) on sort order "name, asc"
		if (currentSort == reqSort)
		{
			bool outOfRange = false;
			char curChar = (char)toupper(getGamelist()->getCursor()->getSortName()[0]);

			// define supported character range
			char startChar = '!';
			char endChar = '_';
			if (curChar < startChar || curChar > endChar)
			{
				// most likely 8 bit ASCII or Unicode (Prefix: 0xc2 or 0xe2) value
				curChar = startChar;
				outOfRange = true;
			}

			mJumpToLetterList = std::make_shared<LetterList>(mWindow, es_translate("JUMP TO ..."), false);
			for (char c = startChar; c <= endChar; c++)
			{
				// check if c is a valid first letter in current list
				const std::vector<FileData*>& files = getGamelist()->getCursor()->getParent()->getChildrenListToDisplay();
				for (auto file : files)
				{
					char candidate = (char)toupper(file->getSortName()[0]);
					if (c == candidate)
					{
						mJumpToLetterList->add(std::string(1, c), c, (c == curChar) || outOfRange);
						outOfRange = false; // only override selection on very first c == candidate match
						break;
					}
				}
			}

			row.elements.clear();
			row.addElement(std::make_shared<TextComponent>(
				mWindow,
				es_translate("JUMP TO ..."),
				Font::get(FONT_SIZE_MEDIUM),
				0x777777FF),
				true);
			row.addElement(mJumpToLetterList, false);
			row.input_handler = [&](InputConfig* config, Input input) {
				if (config->isMappedTo("a", input) && input.value)
				{
					jumpToLetter();
					return true;
				}
				else if (mJumpToLetterList->input(config, input))
				{
					return true;
				}
				return false;
			};
			mMenu.addRow(row);
		}

		// add launch system screensaver
		std::string screensaver_behavior = Settings::getInstance()->getString("ScreenSaverBehavior");
		bool useGamelistMedia = screensaver_behavior == "random video" ||
			(screensaver_behavior == "slideshow" && !Settings::getInstance()->getBool("SlideshowScreenSaverCustomMediaSource"));

		bool rpConfigSelected = "retropie" == mSystem->getName();
		bool collectionsSelected = mSystem->getName() == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName();

		if (!rpConfigSelected && useGamelistMedia && (!collectionsSelected || (collectionsSelected && file->getType() == GAME)))
		{
			row.elements.clear();
			row.addElement(std::make_shared<TextComponent>(
				mWindow,
				es_translate("LAUNCH SYSTEM SCREENSAVER"),
				Font::get(FONT_SIZE_MEDIUM),
				0x777777FF),
				true);
			row.makeAcceptInputHandler(std::bind(&GuiGamelistOptions::launchSystemScreenSaver, this));
			mMenu.addRow(row);
		}

		// "sort list by" menuitem
		mListSort = std::make_shared<SortList>(mWindow, es_translate("SORT GAMES BY"), false);
		for (unsigned int i = 0; i < FileSorts::SortTypes.size(); i++)
		{
			const FileData::SortType& sort = FileSorts::SortTypes.at(i);
			mListSort->add(sort.description, &sort, sort.description == currentSort);
		}

		mMenu.addWithLabel(es_translate("SORT GAMES BY"), mListSort);
	}

	// show filtered menu
	if (!Settings::getInstance()->getBool("ForceDisableFilters"))
	{
		row.elements.clear();
		row.addElement(std::make_shared<TextComponent>(
			mWindow,
			es_translate("FILTER GAMELIST"),
			Font::get(FONT_SIZE_MEDIUM),
			0x777777FF),
			true);
		row.addElement(makeArrow(mWindow), false);
		row.makeAcceptInputHandler(std::bind(&GuiGamelistOptions::openGamelistFilter, this));
		mMenu.addRow(row);
	}

	std::map<std::string, CollectionSystemData> customCollections =
		CollectionSystemManager::get()->getCustomCollectionSystems();

	if (UIModeController::getInstance()->isUIModeFull() &&
		((customCollections.find(system->getName()) != customCollections.cend() &&
		  CollectionSystemManager::get()->getEditingCollection() != system->getName()) ||
		 CollectionSystemManager::get()->getCustomCollectionsBundle()->getName() == system->getName()))
	{
		row.elements.clear();
		row.addElement(std::make_shared<TextComponent>(
			mWindow,
			es_translate("ADD/REMOVE GAMES TO THIS GAME COLLECTION"),
			Font::get(FONT_SIZE_MEDIUM),
			0x777777FF),
			true);
		row.makeAcceptInputHandler(std::bind(&GuiGamelistOptions::startEditMode, this));
		mMenu.addRow(row);
	}

	if (UIModeController::getInstance()->isUIModeFull() && CollectionSystemManager::get()->isEditing())
	{
		row.elements.clear();
		std::string finishText = es_translate("FINISH EDITING") + " '" +
			Utils::String::toUpper(CollectionSystemManager::get()->getEditingCollection()) +
			"' " + es_translate("COLLECTION");
		row.addElement(std::make_shared<TextComponent>(
			mWindow,
			finishText,
			Font::get(FONT_SIZE_MEDIUM),
			0x777777FF),
			true);
		row.makeAcceptInputHandler(std::bind(&GuiGamelistOptions::exitEditMode, this));
		mMenu.addRow(row);
	}

	if (UIModeController::getInstance()->isUIModeFull() && system == CollectionSystemManager::get()->getRandomCollection())
	{
		row.elements.clear();
		row.addElement(std::make_shared<TextComponent>(
			mWindow,
			es_translate("GET NEW RANDOM GAMES"),
			Font::get(FONT_SIZE_MEDIUM),
			0x777777FF),
			true);
		row.makeAcceptInputHandler(std::bind(&GuiGamelistOptions::recreateCollection, this));
		mMenu.addRow(row);
	}

	if (UIModeController::getInstance()->isUIModeFull() && !mFromPlaceholder &&
		!(mSystem->isCollection() && file->getType() == FOLDER))
	{
		row.elements.clear();
		std::string lblTxt;
		if (file->getType() == FOLDER)
			lblTxt = es_translate("EDIT THIS FOLDER'S METADATA");
		else
			lblTxt = es_translate("EDIT THIS GAME'S METADATA");

		row.addElement(std::make_shared<TextComponent>(
			mWindow,
			lblTxt,
			Font::get(FONT_SIZE_MEDIUM),
			0x777777FF),
			true);
		row.addElement(makeArrow(mWindow), false);
		row.makeAcceptInputHandler(std::bind(&GuiGamelistOptions::openMetaDataEd, this));
		mMenu.addRow(row);
	}

	// center the menu
	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	mMenu.setPosition(
		(mSize.x() - mMenu.getSize().x()) / 2,
		(mSize.y() - mMenu.getSize().y()) / 2);
}

GuiGamelistOptions::~GuiGamelistOptions()
{
	FileData* root = mSystem->getRootFolder();
	// apply sort
	if (!mFromPlaceholder)
	{
		const FileData::SortType selectedSort =
			mJumpToSelected
				? FileSorts::SortTypes.at(0) // force "name, asc"
				: *mListSort->getSelected();

		if (root->getSortDescription() != selectedSort.description)
		{
			root->sort(selectedSort); // will also recursively sort children
			getGamelist()->onFileChanged(root, FILE_SORTED);
		}
	}

	if (mFiltersChanged || mMetadataChanged)
	{
		ViewController::get()->getGameListView(mSystem)->setViewportTop(
			TextListComponent<FileData>::REFRESH_LIST_CURSOR_POS);
		ViewController::get()->reloadGameListView(mSystem);
		if (mFiltersChanged)
			getGamelist()->onFileChanged(root, FILE_SORTED);
	}
}

bool GuiGamelistOptions::launchSystemScreenSaver()
{
	SystemData* system = mSystem;
	std::string systemName = system->getName();

	if (systemName == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
	{
		FileData* file = getGamelist()->getCursor(); // is GAME otherwise menuentry would have been hidden
		system = file->getSystem();
	}

	mWindow->startScreenSaver(system);
	mWindow->renderScreenSaver();

	delete this;
	return true;
}

void GuiGamelistOptions::openGamelistFilter()
{
	mFiltersChanged = true;
	GuiGamelistFilter* ggf = new GuiGamelistFilter(mWindow, mSystem);
	mWindow->pushGui(ggf);
}

void GuiGamelistOptions::recreateCollection()
{
	CollectionSystemManager::get()->recreateCollection(mSystem);
	delete this;
}

void GuiGamelistOptions::startEditMode()
{
	std::string editingSystem = mSystem->getName();

	if (editingSystem == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
	{
		FileData* file = getGamelist()->getCursor();
		if (file->getType() == FOLDER)
			editingSystem = file->getName();
		else
			editingSystem = file->getSystem()->getName();
	}
	CollectionSystemManager::get()->setEditMode(editingSystem);
	delete this;
}

void GuiGamelistOptions::exitEditMode()
{
	CollectionSystemManager::get()->exitEditMode();
	delete this;
}

void GuiGamelistOptions::openMetaDataEd()
{
	FileData* file = getGamelist()->getCursor()->getSourceFileData();
	ScraperSearchParams p;
	p.game = file;
	p.system = file->getSystem();

	std::function<void()> saveBtnFunc;
	saveBtnFunc = [this, file] {
		ViewController::get()->getGameListView(mSystem)->setCursor(file, true);
		mMetadataChanged = true;
		ViewController::get()->getGameListView(file->getSystem())->onFileChanged(file, FILE_METADATA_CHANGED);
	};

	std::function<void()> deleteBtnFunc;
	if (file->getType() == FOLDER)
	{
		deleteBtnFunc = NULL;
	}
	else
	{
		deleteBtnFunc = [this, file] {
			CollectionSystemManager::get()->deleteCollectionFiles(file);
			ViewController::get()->getGameListView(file->getSystem()).get()->remove(file, true, true);
		};
	}

	mWindow->pushGui(new GuiMetaDataEd(
		mWindow,
		&file->metadata,
		file->metadata.getMDD(),
		p,
		Utils::FileSystem::getFileName(file->getPath()),
		saveBtnFunc,
		deleteBtnFunc));
}

void GuiGamelistOptions::jumpToLetter()
{
	char letter = mJumpToLetterList->getSelected();
	IGameListView* gamelist = getGamelist();

	const std::vector<FileData*>& files =
		gamelist->getCursor()->getParent()->getChildrenListToDisplay();

	long min = 0;
	long max = (long)files.size() - 1;
	long mid = 0;

	while (max >= min)
	{
		mid = ((max - min) / 2) + min;

		if (files.at(mid)->getName().empty())
			continue;

		char checkLetter = (char)toupper(files.at(mid)->getSortName()[0]);

		if (checkLetter < letter)
			min = mid + 1;
		else if (checkLetter > letter ||
				(mid > 0 && (letter == toupper(files.at(mid - 1)->getSortName()[0]))))
			max = mid - 1;
		else
			break; // exact match found
	}

	gamelist->setCursor(files.at(mid));

	mJumpToSelected = true;

	delete this;
}

bool GuiGamelistOptions::input(InputConfig* config, Input input)
{
	if ((config->isMappedTo("b", input) || config->isMappedTo("select", input)) && input.value)
	{
		delete this;
		return true;
	}

	return mMenu.input(config, input);
}

HelpStyle GuiGamelistOptions::getHelpStyle()
{
	HelpStyle style = HelpStyle();
	style.applyTheme(mSystem->getTheme(), "system");
	return style;
}

std::vector<HelpPrompt> GuiGamelistOptions::getHelpPrompts()
{
	auto prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt("b", es_translate("CLOSE")));
	return prompts;
}

IGameListView* GuiGamelistOptions::getGamelist()
{
	return ViewController::get()->getGameListView(mSystem).get();
}
