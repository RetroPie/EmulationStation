#include "guis/GuiScraperMulti.h"

#include "components/ButtonComponent.h"
#include "components/MenuComponent.h"
#include "components/ScraperSearchComponent.h"
#include "components/TextComponent.h"
#include "guis/GuiMsgBox.h"
#include "views/ViewController.h"
#include "Gamelist.h"
#include "PowerSaver.h"
#include "SystemData.h"
#include "Window.h"
#include "LocaleESHook.h"   // ← traducción (es_translate)

GuiScraperMulti::GuiScraperMulti(Window* window, const std::queue<ScraperSearchParams>& searches, bool approveResults) :
	GuiComponent(window), mBackground(window, ":/frame.png"), mGrid(window, Vector2i(1, 5)),
	mSearchQueue(searches)
{
	assert(mSearchQueue.size());

	addChild(&mBackground);
	addChild(&mGrid);

	PowerSaver::pause();
	mIsProcessing = true;

	mTotalGames = (int)mSearchQueue.size();
	mCurrentGame = 0;
	mTotalSuccessful = 0;
	mTotalSkipped = 0;

	// título
	mTitle = std::make_shared<TextComponent>(
		mWindow,
		es_translate("SCRAPING IN PROGRESS"),
		Font::get(FONT_SIZE_LARGE),
		0x555555FF,
		ALIGN_CENTER);
	mGrid.setEntry(mTitle, Vector2i(0, 0), false, true);

	// sistema
	mSystem = std::make_shared<TextComponent>(
		mWindow,
		es_translate("SYSTEM"),
		Font::get(FONT_SIZE_MEDIUM),
		0x777777FF,
		ALIGN_CENTER);
	mGrid.setEntry(mSystem, Vector2i(0, 1), false, true);

	// subtítulo (se actualiza en doNextSearch)
	mSubtitle = std::make_shared<TextComponent>(
		mWindow,
		"",
		Font::get(FONT_SIZE_SMALL),
		0x888888FF,
		ALIGN_CENTER);
	mGrid.setEntry(mSubtitle, Vector2i(0, 2), false, true);

	// componente de búsqueda
	mSearchComp = std::make_shared<ScraperSearchComponent>(
		mWindow,
		approveResults ? ScraperSearchComponent::ALWAYS_ACCEPT_MATCHING_CRC
		               : ScraperSearchComponent::ALWAYS_ACCEPT_FIRST_RESULT);
	mSearchComp->setAcceptCallback(std::bind(&GuiScraperMulti::acceptResult, this, std::placeholders::_1));
	mSearchComp->setSkipCallback(std::bind(&GuiScraperMulti::skip, this));
	mSearchComp->setCancelCallback(std::bind(&GuiScraperMulti::finish, this));
	mGrid.setEntry(
		mSearchComp,
		Vector2i(0, 3),
		mSearchComp->getSearchType() != ScraperSearchComponent::ALWAYS_ACCEPT_FIRST_RESULT,
		true);

	std::vector< std::shared_ptr<ButtonComponent> > buttons;

	if (approveResults)
	{
		// INPUT
		buttons.push_back(std::make_shared<ButtonComponent>(
			mWindow,
			es_translate("INPUT"),
			es_translate("SEARCH"),
			[&] {
				mSearchComp->openInputScreen(mSearchQueue.front());
				mGrid.resetCursor();
			}));

		// SKIP
		buttons.push_back(std::make_shared<ButtonComponent>(
			mWindow,
			es_translate("SKIP"),
			es_translate("SKIP"),
			[&] {
				skip();
				mGrid.resetCursor();
			}));
	}

	// STOP
	buttons.push_back(std::make_shared<ButtonComponent>(
		mWindow,
		es_translate("STOP"),
		es_translate("STOP (PROGRESS SAVED)"),
		std::bind(&GuiScraperMulti::finish, this)));

	mButtonGrid = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtonGrid, Vector2i(0, 4), true, false);

	setSize(Renderer::getScreenWidth() * 0.95f, Renderer::getScreenHeight() * 0.849f);
	setPosition(
		(Renderer::getScreenWidth() - mSize.x()) / 2,
		(Renderer::getScreenHeight() - mSize.y()) / 2);

	doNextSearch();
}

GuiScraperMulti::~GuiScraperMulti()
{
	// view type probably changed (basic -> detailed)
	for (auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
		ViewController::get()->reloadGameListView(*it, false);
}

void GuiScraperMulti::onSizeChanged()
{
	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mGrid.setRowHeightPerc(0, mTitle->getFont()->getLetterHeight() * 1.9725f / mSize.y(), false);
	mGrid.setRowHeightPerc(1, (mSystem->getFont()->getLetterHeight() + 2) / mSize.y(), false);
	mGrid.setRowHeightPerc(2, mSubtitle->getFont()->getHeight() * 1.75f / mSize.y(), false);
	mGrid.setRowHeightPerc(4, mButtonGrid->getSize().y() / mSize.y(), false);
	mGrid.setSize(mSize);
}

void GuiScraperMulti::doNextSearch()
{
	if (mSearchQueue.empty())
	{
		finish();
		return;
	}

	// sistema actual
	mSystem->setText(Utils::String::toUpper(mSearchQueue.front().system->getFullName()));

	// subtítulo: "GAME X OF Y - NOMBRE.EXT"
	std::stringstream ss;
	std::string gameLabel = es_translate("GAME");
	std::string ofLabel   = es_translate("OF");

	ss << gameLabel << " " << (mCurrentGame + 1)
	   << " " << ofLabel << " " << mTotalGames
	   << " - "
	   << Utils::String::toUpper(Utils::FileSystem::getFileName(mSearchQueue.front().game->getPath()));

	mSubtitle->setText(ss.str());

	mSearchComp->search(mSearchQueue.front());
}

void GuiScraperMulti::acceptResult(const ScraperSearchResult& result)
{
	ScraperSearchParams& search = mSearchQueue.front();

	search.game->metadata = result.mdl;
	updateGamelist(search.system);

	mSearchQueue.pop();
	mCurrentGame++;
	mTotalSuccessful++;
	doNextSearch();
}

void GuiScraperMulti::skip()
{
	mSearchQueue.pop();
	mCurrentGame++;
	mTotalSkipped++;
	doNextSearch();
}

void GuiScraperMulti::finish()
{
	std::stringstream ss;

	if (mTotalSuccessful == 0)
	{
		ss << es_translate("NO GAMES WERE SCRAPED.");
	}
	else
	{
		// X GAME(S) SUCCESSFULLY SCRAPED!
		if (mTotalSuccessful == 1)
			ss << mTotalSuccessful << " " << es_translate("GAME SCRAPED");
		else
			ss << mTotalSuccessful << " " << es_translate("GAMES SCRAPED");

		// Y GAME(S) SKIPPED.
		if (mTotalSkipped > 0)
		{
			ss << "\n";
			if (mTotalSkipped == 1)
				ss << mTotalSkipped << " " << es_translate("GAME SKIPPED");
			else
				ss << mTotalSkipped << " " << es_translate("GAMES SKIPPED");
		}
	}

	mWindow->pushGui(new GuiMsgBox(
		mWindow,
		ss.str(),
		es_translate("OK"),
		[&] { delete this; }));

	mIsProcessing = false;
	PowerSaver::resume();
}

std::vector<HelpPrompt> GuiScraperMulti::getHelpPrompts()
{
	return mGrid.getHelpPrompts();
}
