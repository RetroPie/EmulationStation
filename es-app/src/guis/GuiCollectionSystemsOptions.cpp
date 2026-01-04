#include <fstream>

#include "guis/GuiCollectionSystemsOptions.h"

#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include "components/TextComponent.h"
#include "components/ImageComponent.h"
#include "guis/GuiInfoPopup.h"
#include "guis/GuiRandomCollectionOptions.h"
#include "guis/GuiSettings.h"
#include "guis/GuiTextEditPopup.h"
#include "utils/StringUtil.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "Window.h"
#include "Settings.h"
#include "renderers/Renderer.h"

#include "LocaleES.h"

// Helper local para traducciones .ini
namespace
{
	inline std::string _(const std::string& key)
	{
		return LocaleES::getInstance().translate(key);
	}
}

GuiCollectionSystemsOptions::GuiCollectionSystemsOptions(Window* window)
	: GuiComponent(window)
	, mMenu(window, _("GAME COLLECTION SETTINGS").c_str())
{
	// Asegurar que el idioma actual esté cargado
	LocaleES::getInstance().loadFromSettings();

	initializeMenu();
}

void GuiCollectionSystemsOptions::initializeMenu()
{
	addChild(&mMenu);

	// obtener colecciones
	addSystemsToMenu();

	// opción de ajustes de colección aleatoria
	addEntry(_("RANDOM GAME COLLECTION SETTINGS").c_str(), 0x777777FF, true,
		[this] { openRandomCollectionSettings(); });

	ComponentListRow row;
	if (CollectionSystemManager::get()->isEditing())
	{
		std::string label =
			_("FINISH EDITING") + std::string(" '") +
			Utils::String::toUpper(CollectionSystemManager::get()->getEditingCollection()) +
			std::string("' ") + _("COLLECTION");

		row.addElement(std::make_shared<TextComponent>(
			mWindow,
			label,
			Font::get(FONT_SIZE_MEDIUM),
			0x777777FF),
			true);

		row.makeAcceptInputHandler(std::bind(&GuiCollectionSystemsOptions::exitEditMode, this));
		mMenu.addRow(row);
	}
	else
	{
		// Crear nueva colección desde el tema
		std::vector<std::string> unusedFolders = CollectionSystemManager::get()->getUnusedSystemsFromTheme();
		if (!unusedFolders.empty())
		{
			addEntry(_("CREATE NEW CUSTOM COLLECTION FROM THEME").c_str(), 0x777777FF, true,
				[this, unusedFolders] {
					auto s = new GuiSettings(mWindow, _("SELECT THEME FOLDER").c_str());
					std::shared_ptr< OptionListComponent<std::string> > folderThemes =
						std::make_shared< OptionListComponent<std::string> >(
							mWindow,
							_("SELECT THEME FOLDER"),
							true);

					// añadir sistemas del tema
					for (auto it = unusedFolders.cbegin(); it != unusedFolders.cend(); ++it)
					{
						ComponentListRow row;

						std::string name = *it;
						std::function<void()> createCollectionCall = [name, this, s] {
							createCollection(name);
						};
						row.makeAcceptInputHandler(createCollectionCall);

						auto themeFolder = std::make_shared<TextComponent>(
							mWindow,
							Utils::String::toUpper(name),
							Font::get(FONT_SIZE_SMALL),
							0x777777FF);

						row.addElement(themeFolder, true);
						s->addRow(row);
					}

					mWindow->pushGui(s);
				});
		}

		// Crear nueva colección personalizada
		row.elements.clear();
		row.addElement(std::make_shared<TextComponent>(
			mWindow,
			_("CREATE NEW CUSTOM COLLECTION"),
			Font::get(FONT_SIZE_MEDIUM),
			0x777777FF),
			true);

		auto createCustomCollection = [this](const std::string& newVal) {
			std::string name = newVal;
			// hay que guardar el primer Gui y quitarlo, porque será borrado por el nuevo Gui
			Window* window = mWindow;
			GuiComponent* topGui = window->peekGui();
			window->removeGui(topGui);
			createCollection(name);
		};

		row.makeAcceptInputHandler([this, createCustomCollection] {
			mWindow->pushGui(new GuiTextEditPopup(
				mWindow,
				_("NEW COLLECTION NAME"),
				"",
				createCustomCollection,
				false));
			});
		mMenu.addRow(row);
	}

	// agrupar colecciones sin tema
	bundleCustomCollections = std::make_shared<SwitchComponent>(mWindow);
	bundleCustomCollections->setState(Settings::getInstance()->getBool("UseCustomCollectionsSystem"));
	mMenu.addWithLabel(_("GROUP UNTHEMED CUSTOM COLLECTIONS"), bundleCustomCollections);

	// ordenar colecciones y sistemas
	sortAllSystemsSwitch = std::make_shared<SwitchComponent>(mWindow);
	sortAllSystemsSwitch->setState(Settings::getInstance()->getBool("SortAllSystems"));
	mMenu.addWithLabel(_("SORT CUSTOM COLLECTIONS AND SYSTEMS"), sortAllSystemsSwitch);

	// mostrar nombre del sistema en colecciones
	toggleSystemNameInCollections = std::make_shared<SwitchComponent>(mWindow);
	toggleSystemNameInCollections->setState(Settings::getInstance()->getBool("CollectionShowSystemInfo"));
	mMenu.addWithLabel(_("SHOW SYSTEM NAME IN COLLECTIONS"), toggleSystemNameInCollections);

	// doble pulsación en Y para quitar de favoritos/colección
	doublePressToRemoveFavs = std::make_shared<SwitchComponent>(mWindow);
	doublePressToRemoveFavs->setState(Settings::getInstance()->getBool("DoublePressRemovesFromFavs"));
	mMenu.addWithLabel(_("PRESS (Y) TWICE TO REMOVE FROM FAVS./COLL."), doublePressToRemoveFavs);

	// opción de colección por defecto para screensaver
	defaultScreenSaverCollection =
		std::make_shared< OptionListComponent<std::string> >(
			mWindow,
			_("ADD/REMOVE GAMES WHILE SCREENSAVER TO"),
			false);

	// opción por defecto
	std::string defaultScreenSaverCollectionName =
		Settings::getInstance()->getString("DefaultScreenSaverCollection");
	defaultScreenSaverCollection->add(
		_("<DEFAULT>"),
		"",
		defaultScreenSaverCollectionName == "");

	std::map<std::string, CollectionSystemData> customSystems =
		CollectionSystemManager::get()->getCustomCollectionSystems();

	// añadir sistemas personalizados habilitados
	for (auto it = customSystems.cbegin(); it != customSystems.cend(); ++it)
	{
		if (it->second.isEnabled)
			defaultScreenSaverCollection->add(
				it->second.decl.longName,
				it->second.decl.name,
				defaultScreenSaverCollectionName == it->second.decl.name);
	}

	mMenu.addWithLabel(_("ADD/REMOVE GAMES WHILE SCREENSAVER TO"), defaultScreenSaverCollection);

	// botón volver
	mMenu.addButton(_("BACK"), "back", std::bind(&GuiCollectionSystemsOptions::applySettings, this));

	mMenu.setPosition(
		(Renderer::getScreenWidth() - mMenu.getSize().x()) / 2,
		Renderer::getScreenHeight() * 0.15f);
}

void GuiCollectionSystemsOptions::addEntry(const char* name, unsigned int color, bool add_arrow, const std::function<void()>& func)
{
	std::shared_ptr<Font> font = Font::get(FONT_SIZE_MEDIUM);

	// populate the list
	ComponentListRow row;
	row.addElement(std::make_shared<TextComponent>(mWindow, name, font, color), true);

	if (add_arrow)
	{
		std::shared_ptr<ImageComponent> bracket = makeArrow(mWindow);
		row.addElement(bracket, false);
	}

	row.makeAcceptInputHandler(func);

	mMenu.addRow(row);
}

void GuiCollectionSystemsOptions::createCollection(std::string inName)
{
	CollectionSystemManager* collSysMgr = CollectionSystemManager::get();
	std::string name = collSysMgr->getValidNewCollectionName(inName);

	SystemData* newSys = collSysMgr->addNewCustomCollection(name, true);
	if (!collSysMgr->saveCustomCollection(newSys))
	{
		GuiInfoPopup* s = new GuiInfoPopup(
			mWindow,
			"Failed creating '" + Utils::String::toUpper(name) + "' Collection. See log for details.",
			8000);
		mWindow->setInfoPopup(s);
		return;
	}

	customOptionList->add(name, name, true);
	std::string outAuto = Utils::String::vectorToDelimitedString(autoOptionList->getSelectedObjects(), ",");
	std::string outCustom = Utils::String::vectorToDelimitedString(customOptionList->getSelectedObjects(), ",");
	updateSettings(outAuto, outCustom);
	ViewController::get()->goToSystemView(newSys);

	Window* window = mWindow;
	collSysMgr->setEditMode(name);
	while (window->peekGui() && window->peekGui() != ViewController::get())
		delete window->peekGui();
}

void GuiCollectionSystemsOptions::openRandomCollectionSettings()
{
	mWindow->pushGui(new GuiRandomCollectionOptions(mWindow));
}

void GuiCollectionSystemsOptions::exitEditMode()
{
	CollectionSystemManager::get()->exitEditMode();
	applySettings();
}

GuiCollectionSystemsOptions::~GuiCollectionSystemsOptions()
{
}

void GuiCollectionSystemsOptions::addSystemsToMenu()
{
	std::map<std::string, CollectionSystemData> autoSystems =
		CollectionSystemManager::get()->getAutoCollectionSystems();

	autoOptionList =
		std::make_shared< OptionListComponent<std::string> >(
			mWindow,
			_("SELECT COLLECTIONS"),
			true);

	// añadir sistemas automáticos (ALL GAMES, FAVORITES, RANDOM, LAST PLAYED) con traducción .ini
	for (auto it = autoSystems.cbegin(); it != autoSystems.cend(); ++it)
	{
		// longName viene como "ALL GAMES", "FAVORITES", etc.
		std::string label = it->second.decl.longName;

		// Las claves en el .ini están en minúsculas: "all games", "favorites", "random", "last played"
		std::string lowerKey = Utils::String::toLower(label);
		std::string translated = LocaleES::getInstance().translate(lowerKey);

		if (translated != lowerKey)
			label = translated; // usar la traducción si existe

		autoOptionList->add(
			label,
			it->second.decl.name,
			it->second.isEnabled);
	}
	mMenu.addWithLabel(_("AUTOMATIC GAME COLLECTIONS"), autoOptionList);

	std::map<std::string, CollectionSystemData> customSystems =
		CollectionSystemManager::get()->getCustomCollectionSystems();

	customOptionList =
		std::make_shared< OptionListComponent<std::string> >(
			mWindow,
			_("SELECT COLLECTIONS"),
			true);

	// añadir sistemas personalizados (se muestran con el nombre que les ponga el usuario)
	for (auto it = customSystems.cbegin(); it != customSystems.cend(); ++it)
	{
		customOptionList->add(
			it->second.decl.longName,
			it->second.decl.name,
			it->second.isEnabled);
	}
	mMenu.addWithLabel(_("CUSTOM GAME COLLECTIONS"), customOptionList);
}

void GuiCollectionSystemsOptions::applySettings()
{
	std::string outAuto = Utils::String::vectorToDelimitedString(autoOptionList->getSelectedObjects(), ",");
	std::string prevAuto = Settings::getInstance()->getString("CollectionSystemsAuto");
	std::string outCustom = Utils::String::vectorToDelimitedString(customOptionList->getSelectedObjects(), ",");
	std::string prevCustom = Settings::getInstance()->getString("CollectionSystemsCustom");
	bool outSort = sortAllSystemsSwitch->getState();
	bool prevSort = Settings::getInstance()->getBool("SortAllSystems");
	bool outBundle = bundleCustomCollections->getState();
	bool prevBundle = Settings::getInstance()->getBool("UseCustomCollectionsSystem");
	bool prevShow = Settings::getInstance()->getBool("CollectionShowSystemInfo");
	bool outShow = toggleSystemNameInCollections->getState();

	// comprobar si la colección personalizada sigue habilitada para 'collection during screensaver'
	// si no, ponerla en "" (se muestra como <DEFAULT>)
	std::string enabledCollectionName = "";
	std::vector<std::string> selection = defaultScreenSaverCollection->getSelectedObjects();
	if (!selection.empty())
	{
		std::string selectedCollection = selection.at(0);
		if (!selectedCollection.empty())
		{
			std::vector<std::string> enabledCollections = customOptionList->getSelectedObjects();
			for (auto nameIt = enabledCollections.begin(); nameIt != enabledCollections.end(); ++nameIt)
			{
				if (*nameIt == selectedCollection)
				{
					enabledCollectionName = selectedCollection;
					break;
				}
			}
		}
	}

	Settings::getInstance()->setString("DefaultScreenSaverCollection", enabledCollectionName);
	Settings::getInstance()->setBool("DoublePressRemovesFromFavs", doublePressToRemoveFavs->getState());

	bool needRefreshCollectionSettings =
		prevAuto != outAuto ||
		prevCustom != outCustom ||
		outSort != prevSort ||
		outBundle != prevBundle ||
		prevShow != outShow;

	if (needRefreshCollectionSettings)
	{
		updateSettings(outAuto, outCustom);
	}
	else
	{
		Settings::getInstance()->saveFile();
	}
	delete this;
}

void GuiCollectionSystemsOptions::updateSettings(std::string newAutoSettings, std::string newCustomSettings)
{
	Settings::getInstance()->setString("CollectionSystemsAuto", newAutoSettings);
	Settings::getInstance()->setString("CollectionSystemsCustom", newCustomSettings);
	Settings::getInstance()->setBool("SortAllSystems", sortAllSystemsSwitch->getState());
	Settings::getInstance()->setBool("UseCustomCollectionsSystem", bundleCustomCollections->getState());
	Settings::getInstance()->setBool("CollectionShowSystemInfo", toggleSystemNameInCollections->getState());

	Settings::getInstance()->saveFile();

	CollectionSystemManager::get()->loadEnabledListFromSettings();
	CollectionSystemManager::get()->updateSystemsList();
	ViewController::get()->goToStart();
	ViewController::get()->reloadAll();
}

bool GuiCollectionSystemsOptions::input(InputConfig* config, Input input)
{
	bool consumed = GuiComponent::input(config, input);
	if (consumed)
		return true;

	if (config->isMappedTo("b", input) && input.value != 0)
	{
		applySettings();
	}

	return false;
}

std::vector<HelpPrompt> GuiCollectionSystemsOptions::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt("b", _("BACK")));
	return prompts;
}
