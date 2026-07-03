#include "guis/GuiSearchPopup.h"

#include "components/RatingComponent.h"
#ifdef _OMX_
#include "components/VideoPlayerComponent.h"
#endif
#include "components/VideoVlcComponent.h"
#include "guis/GuiGamelistOptions.h"
#include "guis/GuiMenu.h"
#include "renderers/Renderer.h"
#include "utils/StringUtil.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "FileData.h"
#include "Log.h"
#include "Settings.h"
#include "SystemData.h"
#include "ThemeData.h"
#include "Window.h"
#include <SDL_events.h>
#include <algorithm>
#include <cstring>
#include <cstdlib>  // rand()

GuiSearchPopup::GuiSearchPopup(Window* window, SystemData* scope)
	: GuiComponent(window),
	  mScope(scope), mThemeSystem(nullptr),
	  mSearchText(window), mCharRow(window),
	  mResultList(window), mListMessage(window),
	  mImage(window), mThumbnail(window),
	  mDescContainer(window, 1500), mDescription(window),
	  mRating(window),
	  mDeveloper(window), mPublisher(window), mGenre(window), mPlayers(window),
	  mLblRating(window), mLblDeveloper(window), mLblPublisher(window),
	  mLblGenre(window), mLblPlayers(window),
	  mMarquee(window), mName(window),
	  mReleaseDate(window), mLastPlayed(window), mPlayCount(window),
	  mLblReleaseDate(window), mLblLastPlayed(window), mLblPlayCount(window),
	  mBackground(window),
	  mCursorPos(0), mCancelFlag(false), mResultsReady(false),
	  mFocus(FOCUS_CHAR_ROW), mLastInputWasKeyboard(false),
	  mKeyRepeatKey(0), mKeyRepeatTimer(0), mShoulderRepeatDir(0), mShoulderRepeatTimer(0),
	  mResultListSelectorColor(0x000050FF), mResultListSelectorColorEnd(0x000050FF)
{
	const float sw = (float)Renderer::getScreenWidth();
	const float sh = (float)Renderer::getScreenHeight();
	setSize(sw, sh);

	// ── background (optional, theme-driven, rendered first / behind everything) ─
	mBackground.setDefaultZIndex(0);
	addChild(&mBackground);

	// ── search text ──────────────────────────────────────────────
	mSearchText.setText("");
	mSearchText.setPosition(0, 0);
	mSearchText.setSize(sw, sh * 0.06f);
	mSearchText.setHorizontalAlignment(ALIGN_CENTER);
	mSearchText.setColor(0xCCCCCCFF);
	mSearchText.setFont(Font::get(FONT_SIZE_MEDIUM));
	mSearchText.setDefaultZIndex(40);
	addChild(&mSearchText);

	// ── char row ─────────────────────────────────────────────────
	mCharRow.setPosition(0, sh * 0.06f);
	mCharRow.setSize(sw, sh * 0.07f);
	mCharRow.setDefaultZIndex(40);
	addChild(&mCharRow);

	mCharRow.setCharSelectedCallback([this](const std::string& ch) {
		mQuery.insert(mCursorPos, ch);
		mCursorPos += ch.size();
		updateSearchDisplay();
		startSearch(mQuery);
	});

	mCharRow.setBackspaceCallback([this]()   { editBackspace(); });
	mCharRow.setCursorLeftCallback([this]()  { editCursorLeft(); });
	mCharRow.setCursorRightCallback([this]() { editCursorRight(); });

	// ── result list (left column) ─────────────────────────────────
	mResultList.setPosition(0, sh * 0.13f);
	mResultList.setSize(sw * 0.50f, sh * 0.80f);
	mResultList.setFont(Font::get(FONT_SIZE_SMALL));
	mResultList.setCursorChangedCallback([this](CursorState /*state*/) { updateInfoPanel(); });
	mResultList.setDefaultZIndex(20);
	addChild(&mResultList);

	// ── list message overlay (shown when list is empty) ───────────
	mListMessage.setPosition(0, sh * 0.13f);
	mListMessage.setSize(sw * 0.50f, sh * 0.80f);
	mListMessage.setHorizontalAlignment(ALIGN_CENTER);
	mListMessage.setVerticalAlignment(ALIGN_CENTER);
	mListMessage.setFont(Font::get(FONT_SIZE_SMALL));
	mListMessage.setColor(0x999999FF);
	mListMessage.setDefaultZIndex(25);
	addChild(&mListMessage);

	// ── metadata panel (right column) ────────────────────────────
	const float rx = sw * 0.53f;

	mImage.setOrigin(0.5f, 0.0f);
	mImage.setPosition(rx + sw * 0.225f, sh * 0.13f);
	mImage.setMaxSize(sw * 0.44f, sh * 0.50f);
	mImage.setDefaultZIndex(30);
	addChild(&mImage);

	mThumbnail.setOrigin(0.5f, 0.0f);
	mThumbnail.setPosition(2.0f, 2.0f);
	mThumbnail.setMaxSize(sw * 0.44f, sh * 0.50f);
	mThumbnail.setVisible(false);
	mThumbnail.setDefaultZIndex(35);
	addChild(&mThumbnail);

	// ── video (off-screen / invisible by default; theme must enable it) ───────
#ifdef _OMX_
	if (Settings::getInstance()->getBool("VideoOmxPlayer"))
		mVideo = new VideoPlayerComponent(window, "");
	else
		mVideo = new VideoVlcComponent(window, "");
#else
	mVideo = new VideoVlcComponent(window, "");
#endif
	mVideo->setOrigin(0.5f, 0.0f);
	mVideo->setPosition(2.0f, 2.0f);
	mVideo->setVisible(false);
	mVideo->setDefaultZIndex(30);
	addChild(mVideo);

	// ── marquee (off-screen / invisible by default) ───────────────────────────
	mMarquee.setOrigin(0.5f, 0.0f);
	mMarquee.setPosition(2.0f, 2.0f);
	mMarquee.setVisible(false);
	mMarquee.setDefaultZIndex(35);
	addChild(&mMarquee);

	// ── name (off-screen by default, theme overrides position) ────────────────
	mName.setPosition(sw, sh);
	mName.setDefaultZIndex(40);
	mName.setColor(0xAAAAAAFF);
	mName.setFont(Font::get(FONT_SIZE_MEDIUM));
	mName.setHorizontalAlignment(ALIGN_CENTER);
	addChild(&mName);

	mDescContainer.setPosition(rx, sh * 0.72f);
	mDescContainer.setSize(sw * 0.44f, sh * 0.20f);
	mDescContainer.setAutoScroll(true);
	mDescContainer.setDefaultZIndex(40);
	addChild(&mDescContainer);

	mDescription.setFont(Font::get(FONT_SIZE_SMALL));
	mDescription.setColor(0xDDDDDDFF);
	mDescription.setSize(sw * 0.44f, 0);
	mDescContainer.addChild(&mDescription);

	// stacked metadata rows below description
	const float mh = sh * 0.04f;
	std::shared_ptr<Font> smallFont = Font::get(FONT_SIZE_SMALL);

	auto placeLbl = [&](TextComponent& lbl, const char* text, float y) {
		lbl.setText(text);
		lbl.setFont(smallFont);
		lbl.setColor(0x999999FF);
		lbl.setPosition(rx, y);
		lbl.setSize(sw * 0.12f, mh);
		lbl.setDefaultZIndex(40);
		addChild(&lbl);
	};
	auto placeVal = [&](GuiComponent& val, float y) {
		val.setPosition(rx + sw * 0.13f, y);
		val.setSize(sw * 0.30f, mh);
		val.setDefaultZIndex(40);
		addChild(&val);
	};

	float my = sh * 0.63f;
	placeLbl(mLblRating,      "Rating: ",       my); placeVal(mRating,      my); my += mh;
	placeLbl(mLblDeveloper,   "Developer: ",    my); placeVal(mDeveloper,   my); my += mh;
	placeLbl(mLblGenre,       "Genre: ",        my); placeVal(mGenre,       my); my += mh;
	placeLbl(mLblPlayers,     "Players: ",      my); placeVal(mPlayers,     my); my += mh;
	placeLbl(mLblPublisher,   "Publisher: ",    my); placeVal(mPublisher,   my); my += mh;
	placeLbl(mLblReleaseDate, "Released: ",     my); placeVal(mReleaseDate, my); my += mh;
	placeLbl(mLblLastPlayed,  "Last played: ",  my); placeVal(mLastPlayed,  my); my += mh;
	placeLbl(mLblPlayCount,   "Times played: ", my); placeVal(mPlayCount,   my);

	mLastPlayed.setDisplayRelative(true);

	mDeveloper.setFont(smallFont);
	mDeveloper.setColor(0xDDDDDDFF);
	mGenre.setFont(smallFont);
	mGenre.setColor(0xDDDDDDFF);
	mPlayers.setFont(smallFont);
	mPlayers.setColor(0xDDDDDDFF);
	mPublisher.setFont(smallFont);
	mPublisher.setColor(0xDDDDDDFF);
	mReleaseDate.setFont(smallFont);
	mReleaseDate.setColor(0xDDDDDDFF);
	mLastPlayed.setFont(smallFont);
	mLastPlayed.setColor(0xDDDDDDFF);
	mPlayCount.setFont(smallFont);
	mPlayCount.setColor(0xDDDDDDFF);

	// Apply theme from scope system (or first available system for all-systems search)
	SystemData* themeSource = mScope;
	if (!themeSource && !SystemData::sSystemVector.empty())
		themeSource = SystemData::sSystemVector.front();
	if (themeSource)
	{
		applyTheme(themeSource);
		// Apply result list theme once here — never re-applied so scrolling doesn't flicker
		auto theme = themeSource->getTheme();
		mResultList.applyTheme(theme, "search", "gamelist", ThemeFlags::ALL);
		const ThemeData::ThemeElement* gamelistElem =
			theme->getElement("search", "gamelist", "textlist");
		if (gamelistElem)
		{
			if (gamelistElem->has("selectorColor"))
				mResultListSelectorColor = gamelistElem->get<unsigned int>("selectorColor");
			if (gamelistElem->has("selectorColorEnd"))
				mResultListSelectorColorEnd = gamelistElem->get<unsigned int>("selectorColorEnd");
		}
	}

	buildGameCache();
	mListMessage.setText("TYPE TO SEARCH...");
	updateFocusVisuals();

	// Discard any SDL_TEXTINPUT events queued by the key that opened this popup
	SDL_FlushEvent(SDL_TEXTINPUT);
}

GuiSearchPopup::~GuiSearchPopup()
{
	cancelSearch();
	delete mVideo;
}

void GuiSearchPopup::applyTheme(SystemData* sys)
{
	mThemeSystem = sys;
	if (!sys)
		return;

	auto theme = sys->getTheme();
	using namespace ThemeFlags;

	// Background (optional, renders behind everything)
	mBackground.applyTheme(theme, "search", "background", ALL);

	// Image / thumbnail
	mImage.applyTheme(    theme, "search", "md_image",     POSITION | SIZE | Z_INDEX | ROTATION | VISIBLE);
	mThumbnail.applyTheme(theme, "search", "md_thumbnail", POSITION | SIZE | Z_INDEX | ROTATION | VISIBLE);

	// Video (invisible by default; theme must set visible=true to enable)
	mVideo->applyTheme(theme, "search", "md_video",
		POSITION | SIZE | DELAY | Z_INDEX | ROTATION | VISIBLE);

	// Marquee
	mMarquee.applyTheme(theme, "search", "md_marquee",
		POSITION | SIZE | Z_INDEX | ROTATION | VISIBLE);

	// Name
	mName.applyTheme(theme, "search", "md_name", ALL);

	// Description
	mDescContainer.applyTheme(theme, "search", "md_description", POSITION | SIZE | Z_INDEX | VISIBLE);
	mDescription.setSize(mDescContainer.getSize().x(), 0);
	mDescription.applyTheme(theme, "search", "md_description",
		ALL ^ (POSITION | SIZE | ORIGIN | TEXT | ROTATION));

	// Metadata values
	mRating.applyTheme(     theme, "search", "md_rating",      ALL ^ TEXT);
	mDeveloper.applyTheme(  theme, "search", "md_developer",   ALL ^ TEXT);
	mPublisher.applyTheme(  theme, "search", "md_publisher",   ALL ^ TEXT);
	mGenre.applyTheme(      theme, "search", "md_genre",       ALL ^ TEXT);
	mPlayers.applyTheme(    theme, "search", "md_players",     ALL ^ TEXT);
	mReleaseDate.applyTheme(theme, "search", "md_releasedate", ALL ^ TEXT);
	mLastPlayed.applyTheme( theme, "search", "md_lastplayed",  ALL ^ TEXT);
	mPlayCount.applyTheme(  theme, "search", "md_playcount",   ALL ^ TEXT);

	// Labels
	mLblRating.applyTheme(     theme, "search", "md_lbl_rating",      ALL);
	mLblDeveloper.applyTheme(  theme, "search", "md_lbl_developer",   ALL);
	mLblPublisher.applyTheme(  theme, "search", "md_lbl_publisher",   ALL);
	mLblGenre.applyTheme(      theme, "search", "md_lbl_genre",       ALL);
	mLblPlayers.applyTheme(    theme, "search", "md_lbl_players",     ALL);
	mLblReleaseDate.applyTheme(theme, "search", "md_lbl_releasedate", ALL);
	mLblLastPlayed.applyTheme( theme, "search", "md_lbl_lastplayed",  ALL);
	mLblPlayCount.applyTheme(  theme, "search", "md_lbl_playcount",   ALL);

	// Layout elements (no-op if element not defined in theme)
	mSearchText.applyTheme( theme, "search", "searchtext",  ALL ^ TEXT);
	mListMessage.applyTheme(theme, "search", "listmessage", ALL ^ TEXT);

	// Note: mResultList theme is applied once in the constructor — not here —
	// to avoid flickering the favorite icon on every cursor change.
}

void GuiSearchPopup::updateSearchDisplay()
{
	std::string display = mQuery.substr(0, mCursorPos) + "|" + mQuery.substr(mCursorPos);
	mSearchText.setText(display);
}

void GuiSearchPopup::updateInfoPanel()
{
	if (mResultList.size() == 0)
		return;

	FileData* file = mResultList.getSelected();
	if (!file || file->getType() != GAME)
	{
		clearInfoPanel();
		return;
	}

	// Dynamic theme reload when cursor moves to a game from a different system.
	// mResultList is intentionally excluded from applyTheme() to avoid flickering.
	SystemData* sys = file->getSystem();
	if (sys != mThemeSystem)
		applyTheme(sys);

	mImage.setImage(file->getImagePath());
	mThumbnail.setImage(file->getThumbnailPath());

	if (mVideo->isVisible())
	{
		if (!mVideo->setVideo(file->getVideoPath()))
			mVideo->setDefaultVideo();
		mVideo->setImage(file->getThumbnailPath());
	}

	mMarquee.setImage(file->getMarqueePath());
	mName.setText(file->getName());

	mDescription.setText(file->metadata.get("desc"));
	mDescription.setSize(mDescription.getSize().x(), 0); // auto-height
	mDescContainer.reset();

	mRating.setValue(file->metadata.get("rating"));
	mDeveloper.setValue(file->metadata.get("developer"));
	mPublisher.setValue(file->metadata.get("publisher"));
	mGenre.setValue(file->metadata.get("genre"));
	mPlayers.setValue(file->metadata.get("players"));
	mReleaseDate.setValue(file->metadata.get("releasedate"));
	mLastPlayed.setValue(file->metadata.get("lastplayed"));
	mPlayCount.setValue(file->metadata.get("playcount"));
}

void GuiSearchPopup::editBackspace()
{
	if (mCursorPos > 0)
	{
		size_t prev = Utils::String::prevCursor(mQuery, mCursorPos);
		mQuery.erase(prev, mCursorPos - prev);
		mCursorPos = prev;
	}
	updateSearchDisplay();
	startSearch(mQuery);
}

void GuiSearchPopup::editCursorLeft()
{
	if (mCursorPos > 0)
		mCursorPos = Utils::String::prevCursor(mQuery, mCursorPos);
	updateSearchDisplay();
}

void GuiSearchPopup::editCursorRight()
{
	if (mCursorPos < mQuery.size())
		mCursorPos = Utils::String::nextCursor(mQuery, mCursorPos);
	updateSearchDisplay();
}

void GuiSearchPopup::buildGameCache()
{
	mAllGames.clear();
	mLowerNames.clear();

	auto addSystem = [&](SystemData* sys) {
		if (!sys->isGameSystem() || sys->isCollection())
			return;
		for (auto game : sys->getRootFolder()->getFilesRecursive(GAME))
		{
			mAllGames.push_back(game);
			mLowerNames.push_back(Utils::String::toLower(game->getName()));
		}
	};

	if (mScope)
	{
		addSystem(mScope);
	}
	else
	{
		for (auto sys : SystemData::sSystemVector)
			addSystem(sys);
	}

	LOG(LogInfo) << "GuiSearchPopup: cached " << mAllGames.size() << " games";
}

void GuiSearchPopup::startSearch(const std::string& query)
{
	cancelSearch();

	if (query.empty())
	{
		mResultList.clear();
		mCurrentResults.clear();
		addPlaceholder("TYPE TO SEARCH...");
		clearInfoPanel();
		return;
	}

	mCancelFlag.store(false);
	mResultsReady.store(false);

	std::string lowerQuery = Utils::String::toLower(query);

	mSearchThread = std::thread([this, lowerQuery]() {
		std::vector<FileData*> results;
		for (size_t i = 0; i < mAllGames.size(); i++)
		{
			if (mCancelFlag.load())
				return;
			if (mLowerNames[i].find(lowerQuery) != std::string::npos)
				results.push_back(mAllGames[i]);
		}
		std::sort(results.begin(), results.end(), [](FileData* a, FileData* b) {
			return Utils::String::toLower(a->getName()) < Utils::String::toLower(b->getName());
		});
		{
			std::lock_guard<std::mutex> lock(mResultMutex);
			mPendingResults = std::move(results);
		}
		mResultsReady.store(true);
	});
}

void GuiSearchPopup::cancelSearch()
{
	mCancelFlag.store(true);
	if (mSearchThread.joinable())
		mSearchThread.join();
}

void GuiSearchPopup::populateResultsList(const std::vector<FileData*>& results)
{
	mResultList.clear();
	mCurrentResults.clear();

	if (results.empty())
	{
		addPlaceholder("NO RESULTS FOUND");
		clearInfoPanel();
		return;
	}

	mListMessage.setText("");

	for (auto game : results)
	{
		std::string displayName = game->getName();
		if (!mScope)
			displayName += " [" + Utils::String::toUpper(game->getSystem()->getName()) + "]";
		mResultList.add(displayName, game, 0);
		mCurrentResults.push_back(game);
	}
}

void GuiSearchPopup::addPlaceholder(const std::string& text)
{
	mListMessage.setText(text);
}

void GuiSearchPopup::clearInfoPanel()
{
	mImage.setImage("");
	mThumbnail.setImage("");
	mVideo->setVideo("");
	mVideo->setImage("");
	mMarquee.setImage("");
	mName.setText("");
	mDescription.setText("");
	mRating.setValue("0");
	mDeveloper.setValue("");
	mPublisher.setValue("");
	mGenre.setValue("");
	mPlayers.setValue("");
	mReleaseDate.setValue("");
	mLastPlayed.setValue("");
	mPlayCount.setValue("");
}

void GuiSearchPopup::updateFocusVisuals()
{
	bool charRowFocused = (mFocus == FOCUS_CHAR_ROW);
	mCharRow.setFocused(charRowFocused);
	if (!charRowFocused)
	{
		mKeyRepeatKey = 0;      // stop keyboard held-key repeat
		mShoulderRepeatDir = 0; // stop gamepad shoulder cursor repeat
	}

	if (charRowFocused)
	{
		mResultList.setSelectorColor(0x00000000);
		mResultList.setSelectorColorEnd(0x00000000);
	}
	else
	{
		mResultList.setSelectorColor(mResultListSelectorColor);
		mResultList.setSelectorColorEnd(mResultListSelectorColorEnd);
	}

	// Refresh the help bar to reflect the current focus state
	updateHelpPrompts();
}

void GuiSearchPopup::launch(FileData* game)
{
	ViewController::get()->launch(game);
}

void GuiSearchPopup::update(int deltaTime)
{
	// Keyboard held-key repeat (backspace / delete / cursor arrows)
	if (mKeyRepeatKey != 0 && mFocus == FOCUS_CHAR_ROW)
	{
		mKeyRepeatTimer += deltaTime;
		while (mKeyRepeatTimer >= KEY_REPEAT_DELAY_MS)
		{
			mKeyRepeatTimer -= KEY_REPEAT_PERIOD_MS;
			if (mKeyRepeatKey == SDLK_BACKSPACE)
			{
				editBackspace();
			}
			else if (mKeyRepeatKey == SDLK_DELETE && mCursorPos < mQuery.size())
			{
				size_t next = Utils::String::nextCursor(mQuery, mCursorPos);
				mQuery.erase(mCursorPos, next - mCursorPos);
				updateSearchDisplay();
				startSearch(mQuery);
			}
			else if (mKeyRepeatKey == SDLK_LEFT)  editCursorLeft();
			else if (mKeyRepeatKey == SDLK_RIGHT) editCursorRight();
		}
	}

	// Gamepad shoulder button cursor repeat
	if (mShoulderRepeatDir != 0 && mFocus == FOCUS_CHAR_ROW)
	{
		mShoulderRepeatTimer += deltaTime;
		while (mShoulderRepeatTimer >= KEY_REPEAT_DELAY_MS)
		{
			mShoulderRepeatTimer -= KEY_REPEAT_PERIOD_MS;
			if (mShoulderRepeatDir < 0) editCursorLeft();
			else                        editCursorRight();
		}
	}

	if (mResultsReady.load())
	{
		std::vector<FileData*> results;
		{
			std::lock_guard<std::mutex> lock(mResultMutex);
			results = std::move(mPendingResults);
		}
		mResultsReady.store(false);
		populateResultsList(results);
		updateInfoPanel();
	}

	mVideo->update(deltaTime);
	GuiComponent::update(deltaTime);
}

bool GuiSearchPopup::input(InputConfig* config, Input input)
{
	const bool isKeyboard = (config->getDeviceId() == DEVICE_KEYBOARD);

	// Refresh help bar whenever the active input device type changes
	if (isKeyboard != mLastInputWasKeyboard)
	{
		mLastInputWasKeyboard = isKeyboard;
		updateHelpPrompts();
	}

	if (input.value != 0)
	{
		// ── Keyboard in char row: intercept all keys to avoid button-map conflicts ─
		// In the result list, keyboard uses the normal button map (same as gamepad).
		if (isKeyboard && mFocus == FOCUS_CHAR_ROW)
		{
			if (input.id == SDLK_ESCAPE)
			{
				cancelSearch();
				delete this;
				return true;
			}
			if (input.id == SDLK_DOWN && !mCurrentResults.empty())
			{
				mFocus = FOCUS_RESULT_LIST;
				mResultList.setCursorIndex(0);
				updateFocusVisuals();
				return true;
			}
			if (input.id == SDLK_SPACE)
			{
				mQuery.insert(mCursorPos, " ");
				mCursorPos++;
				updateSearchDisplay();
				startSearch(mQuery);
				return true;
			}
			if (input.id == SDLK_BACKSPACE)
			{
				mKeyRepeatKey = SDLK_BACKSPACE; mKeyRepeatTimer = 0;
				editBackspace();
				return true;
			}
			if (input.id == SDLK_LEFT)  { mKeyRepeatKey = SDLK_LEFT;  mKeyRepeatTimer = 0; editCursorLeft();  return true; }
			if (input.id == SDLK_RIGHT) { mKeyRepeatKey = SDLK_RIGHT; mKeyRepeatTimer = 0; editCursorRight(); return true; }
			if (input.id == SDLK_DELETE)
			{
				mKeyRepeatKey = SDLK_DELETE; mKeyRepeatTimer = 0;
				if (mCursorPos < mQuery.size())
				{
					size_t next = Utils::String::nextCursor(mQuery, mCursorPos);
					mQuery.erase(mCursorPos, next - mCursorPos);
				}
				updateSearchDisplay();
				startSearch(mQuery);
				return true;
			}
			if (input.id == SDLK_HOME) { mCursorPos = 0;             updateSearchDisplay(); return true; }
			if (input.id == SDLK_END)  { mCursorPos = mQuery.size(); updateSearchDisplay(); return true; }
			// All other keys: let SDL_TEXTINPUT handle printable chars
			return false;
		}

		// ── Button-mapped handling (gamepad + keyboard in result list) ────────────

		// Close on B (or Escape on keyboard — Escape isn't in the button map)
		if (config->isMappedTo("b", input) || (isKeyboard && input.id == SDLK_ESCAPE))
		{
			cancelSearch();
			delete this;
			return true;
		}

		// RT: move focus back to char row
		if (config->isMappedTo("righttrigger", input))
		{
			if (mFocus == FOCUS_RESULT_LIST)
			{
				mResultList.stopScrolling(true);
				mFocus = FOCUS_CHAR_ROW;
				updateFocusVisuals();
				SDL_FlushEvent(SDL_TEXTINPUT); // discard text from the key that triggered this
			}
			return true;
		}

		// Main menu
		if (config->isMappedTo("start", input) &&
		    !(UIModeController::getInstance()->isUIModeKid() &&
		      Settings::getInstance()->getBool("DisableKidStartMenu")))
		{
			mWindow->pushGui(new GuiMenu(mWindow));
			return true;
		}

		// Result list actions
		if (mFocus == FOCUS_RESULT_LIST)
		{
			FileData* selected = (mResultList.size() > 0) ? mResultList.getSelected() : nullptr;
			bool hasGame = selected && selected->getType() == GAME;

			if (config->isMappedTo("y", input) && !UIModeController::getInstance()->isUIModeKid())
			{
				if (hasGame)
					CollectionSystemManager::get()->toggleGameInCollection(selected);
				return true;
			}
			if (config->isMappedTo("x", input))
			{
				if (mResultList.size() > 1)
				{
					int idx = std::rand() % mResultList.size();
					mResultList.setCursorIndex(idx);
				}
				return true;
			}
			if (config->isMappedTo("select", input) && !UIModeController::getInstance()->isUIModeKid())
			{
				SystemData* sys = hasGame ? selected->getSystem() : mScope;
				if (!sys && !SystemData::sSystemVector.empty())
					sys = SystemData::sSystemVector.front();
				if (sys)
				{
					auto jumpFiles = mCurrentResults;
					mWindow->pushGui(new GuiGamelistOptions(mWindow, sys, jumpFiles,
						[this](int idx) { mResultList.setCursorIndex(idx); }));
				}
				return true;
			}
		}

		if (mFocus == FOCUS_CHAR_ROW)
		{
			// Gamepad only here — keyboard char row is handled above
			if (config->isMappedLike("leftshoulder", input))  { mShoulderRepeatDir = -1; mShoulderRepeatTimer = 0; editCursorLeft();  return true; }
			if (config->isMappedLike("rightshoulder", input)) { mShoulderRepeatDir =  1; mShoulderRepeatTimer = 0; editCursorRight(); return true; }
			if (config->isMappedLike("down", input) && !mCurrentResults.empty())
			{
				mFocus = FOCUS_RESULT_LIST;
				updateFocusVisuals();
				return true;
			}
			if (mCharRow.input(config, input))
				return true;
		}
		else // FOCUS_RESULT_LIST
		{
			if (config->isMappedLike("up", input))
			{
				if (mResultList.getCursorIndex() == 0)
				{
					mResultList.stopScrolling(true);
					mFocus = FOCUS_CHAR_ROW;
					updateFocusVisuals();
					return true;
				}
				mResultList.input(config, input);
				return true;
			}
			if (config->isMappedLike("down", input))
			{
				if (mResultList.size() == 0 ||
				    mResultList.getCursorIndex() == (int)mResultList.size() - 1)
				{
					mResultList.stopScrolling(true);
					mFocus = FOCUS_CHAR_ROW;
					updateFocusVisuals();
					return true;
				}
				mResultList.input(config, input);
				return true;
			}
			if (config->isMappedTo("a", input))
			{
				FileData* cursor = mResultList.size() > 0 ? mResultList.getSelected() : nullptr;
				if (cursor && cursor->getType() == GAME)
					launch(cursor);
				return true;
			}
			if (mResultList.input(config, input))
				return true;
		}
	}
	else // value == 0 (release)
	{
		if (isKeyboard && mFocus == FOCUS_CHAR_ROW)
		{
			if ((int)input.id == mKeyRepeatKey)
				mKeyRepeatKey = 0;
		}
		else if (mFocus == FOCUS_RESULT_LIST)
			mResultList.input(config, input);
		else if (mFocus == FOCUS_CHAR_ROW)
		{
			if (mShoulderRepeatDir != 0 &&
			    (config->isMappedLike("leftshoulder", input) || config->isMappedLike("rightshoulder", input)))
				mShoulderRepeatDir = 0;
			mCharRow.input(config, input); // stops gamepad scroll/backspace repeat
		}
	}

	return GuiComponent::input(config, input);
}

void GuiSearchPopup::textInput(const char* text)
{
	// Reject control characters and space (space is handled via SDLK_SPACE in input())
	if ((unsigned char)text[0] <= 0x20)
		return;

	if (mFocus != FOCUS_CHAR_ROW)
		return;

	mQuery.insert(mCursorPos, text);
	mCursorPos += strlen(text);
	updateSearchDisplay();
	startSearch(mQuery);
}

void GuiSearchPopup::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	// Dark background panel
	Renderer::setMatrix(trans);
	Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0x000000E0, 0x000000E0);

	renderChildren(trans);
}

std::vector<HelpPrompt> GuiSearchPopup::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;

	if (mLastInputWasKeyboard && mFocus == FOCUS_CHAR_ROW)
	{
		// Keyboard char row: show keyboard-native hints
		prompts.push_back(HelpPrompt("up/down", "results"));
		prompts.push_back(HelpPrompt("esc", "close"));
	}
	else
	{
		// Gamepad, or keyboard in result list (uses button map)
		if (mFocus == FOCUS_CHAR_ROW)
		{
			prompts.push_back(HelpPrompt("lr", "cursor"));
			prompts.push_back(HelpPrompt("left/right", "choose"));
			prompts.push_back(HelpPrompt("a", "type"));
			prompts.push_back(HelpPrompt("x", "backspace"));
			prompts.push_back(HelpPrompt("up/down", "results"));
		}
		else
		{
			prompts.push_back(HelpPrompt("up/down", "choose"));
			prompts.push_back(HelpPrompt("lr", "page"));
			prompts.push_back(HelpPrompt("a", "launch"));
			prompts.push_back(HelpPrompt("x", "random"));
			if (!UIModeController::getInstance()->isUIModeKid())
			{
				prompts.push_back(HelpPrompt("y", "favorite"));
				prompts.push_back(HelpPrompt("select", "options"));
			}
		}
		prompts.push_back(HelpPrompt("b", "close"));
		if (!UIModeController::getInstance()->isUIModeKid())
			prompts.push_back(HelpPrompt("start", "menu"));
		if (mFocus == FOCUS_RESULT_LIST)
			prompts.push_back(HelpPrompt("rt", "keyboard"));
	}

	return prompts;
}
