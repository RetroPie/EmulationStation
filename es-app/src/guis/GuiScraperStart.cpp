#include "guis/GuiScraperStart.h"

#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiScraperMulti.h"
#include "views/ViewController.h"
#include "FileData.h"
#include "SystemData.h"
#include "LocaleESHook.h"   // 🔹 Traducción disponible (ruta correcta)

GuiScraperStart::GuiScraperStart(Window* window) : GuiComponent(window),
    mMenu(window, es_translate("SCRAPE NOW").c_str())
{
    addChild(&mMenu);

    // Filters
    mFilters = std::make_shared< OptionListComponent<GameFilterFunc> >(mWindow, es_translate("SCRAPE THESE GAMES"), false);
    mFilters->add(es_translate("ALL GAMES"),
        [](SystemData*, FileData*) -> bool { return true; }, false);
    mFilters->add(es_translate("ONLY MISSING IMAGE"),
        [](SystemData*, FileData* g) -> bool { return g->metadata.get("image").empty(); }, true);
    mMenu.addWithLabel(es_translate("FILTER"), mFilters);

    // Systems
    mSystems = std::make_shared< OptionListComponent<SystemData*> >(mWindow, es_translate("SCRAPE THESE SYSTEMS"), true);
    for(auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
    {
        if(!(*it)->hasPlatformId(PlatformIds::PLATFORM_IGNORE))
            mSystems->add((*it)->getFullName(), *it, !(*it)->getPlatformIds().empty());
    }
    mMenu.addWithLabel(es_translate("SYSTEMS"), mSystems);

    // Conflicts
    mApproveResults = std::make_shared<SwitchComponent>(mWindow);
    mApproveResults->setState(true);
    mMenu.addWithLabel(es_translate("USER DECIDES ON CONFLICTS"), mApproveResults);

    // Buttons
    mMenu.addButton(es_translate("START"), es_translate("START"), std::bind(&GuiScraperStart::pressedStart, this));
    mMenu.addButton(es_translate("BACK"), es_translate("BACK"), [&] { delete this; });

    mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2,
                      Renderer::getScreenHeight() * 0.15f);
}

void GuiScraperStart::pressedStart()
{
    std::vector<SystemData*> sys = mSystems->getSelectedObjects();
    for(auto it = sys.cbegin(); it != sys.cend(); it++)
    {
        if((*it)->getPlatformIds().empty())
        {
            mWindow->pushGui(new GuiMsgBox(mWindow,
                es_translate("WARNING: SOME SELECTED SYSTEMS DO NOT HAVE A PLATFORM SET. RESULTS MAY BE INACCURATE.\nCONTINUE ANYWAY?"),
                es_translate("YES"), std::bind(&GuiScraperStart::start, this),
                es_translate("NO"), nullptr));
            return;
        }
    }

    start();
}

void GuiScraperStart::start()
{
    std::queue<ScraperSearchParams> searches = getSearches(mSystems->getSelectedObjects(), mFilters->getSelected());

    if(searches.empty())
    {
        mWindow->pushGui(new GuiMsgBox(mWindow, es_translate("NO GAMES FIT THAT CRITERIA.")));
    }
    else
    {
        GuiScraperMulti* gsm = new GuiScraperMulti(mWindow, searches, mApproveResults->getState());
        mWindow->pushGui(gsm);
        delete this;
    }
}

std::queue<ScraperSearchParams> GuiScraperStart::getSearches(std::vector<SystemData*> systems, GameFilterFunc selector)
{
    std::queue<ScraperSearchParams> queue;
    for(auto sys = systems.cbegin(); sys != systems.cend(); sys++)
    {
        std::vector<FileData*> games = (*sys)->getRootFolder()->getFilesRecursive(GAME);
        for(auto game = games.cbegin(); game != games.cend(); game++)
        {
            if(selector((*sys), (*game)))
            {
                ScraperSearchParams search;
                search.game = *game;
                search.system = *sys;
                queue.push(search);
            }
        }
    }

    return queue;
}

bool GuiScraperStart::input(InputConfig* config, Input input)
{
    bool consumed = GuiComponent::input(config, input);
    if(consumed)
        return true;

    if(input.value != 0 && config->isMappedTo("b", input))
    {
        delete this;
        return true;
    }

    if(config->isMappedTo("start", input) && input.value != 0)
    {
        // close everything
        Window* window = mWindow;
        while(window->peekGui() && window->peekGui() != ViewController::get())
            delete window->peekGui();
    }

    return false;
}

std::vector<HelpPrompt> GuiScraperStart::getHelpPrompts()
{
    std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
    prompts.push_back(HelpPrompt("b", es_translate("BACK")));
    prompts.push_back(HelpPrompt("start", es_translate("CLOSE")));
    return prompts;
}
