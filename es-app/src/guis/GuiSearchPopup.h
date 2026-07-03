#pragma once
#ifndef ES_APP_GUIS_GUI_SEARCH_POPUP_H
#define ES_APP_GUIS_GUI_SEARCH_POPUP_H

#include "components/CharacterRowComponent.h"
#include "components/DateTimeComponent.h"
#include "components/ImageComponent.h"
#include "components/ScrollableContainer.h"
#include "components/TextComponent.h"
#include "components/TextListComponent.h"
#include "components/RatingComponent.h"
#include "components/VideoComponent.h"
#include "GuiComponent.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class FileData;
class SystemData;

class GuiSearchPopup : public GuiComponent
{
public:
	// scope == nullptr → search all systems
	// scope != nullptr → search within that system only
	GuiSearchPopup(Window* window, SystemData* scope);
	~GuiSearchPopup();

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	void textInput(const char* text) override;
	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void buildGameCache();
	void updateSearchDisplay();
	void updateInfoPanel();
	void applyTheme(SystemData* sys);
	void startSearch(const std::string& query);
	void cancelSearch();
	void populateResultsList(const std::vector<FileData*>& results);
	void addPlaceholder(const std::string& text);
	void updateFocusVisuals();
	void launch(FileData* game);

	// Edit operations — shared by char row callbacks and physical keyboard handlers
	void editBackspace();
	void editCursorLeft();
	void editCursorRight();

	SystemData* mScope;       // nullptr = all systems
	SystemData* mThemeSystem; // system whose theme is currently applied

	void clearInfoPanel();

	// Search input
	TextComponent mSearchText;
	CharacterRowComponent mCharRow;
	TextListComponent<FileData*> mResultList;
	TextComponent mListMessage;

	// Metadata display (right panel)
	ImageComponent    mImage;
	ImageComponent    mThumbnail;
	ScrollableContainer mDescContainer;
	TextComponent     mDescription;
	RatingComponent   mRating;
	TextComponent     mDeveloper;
	TextComponent     mPublisher;
	TextComponent     mGenre;
	TextComponent     mPlayers;
	TextComponent     mLblRating;
	TextComponent     mLblDeveloper;
	TextComponent     mLblPublisher;
	TextComponent     mLblGenre;
	TextComponent     mLblPlayers;

	// Extended metadata (theme-driven, off-screen by default)
	VideoComponent*   mVideo;
	ImageComponent    mMarquee;
	TextComponent     mName;
	DateTimeComponent mReleaseDate;
	DateTimeComponent mLastPlayed;
	TextComponent     mPlayCount;
	TextComponent     mLblReleaseDate;
	TextComponent     mLblLastPlayed;
	TextComponent     mLblPlayCount;
	ImageComponent    mBackground;

	// Search state
	std::string mQuery;
	size_t mCursorPos;
	std::vector<FileData*> mAllGames;
	std::vector<std::string> mLowerNames;
	std::vector<FileData*> mCurrentResults;  // last populated result set (for jump-to)

	// Threading
	std::thread mSearchThread;
	std::atomic<bool> mCancelFlag;
	std::mutex mResultMutex;
	std::vector<FileData*> mPendingResults;
	std::atomic<bool> mResultsReady;

	// Focus
	enum FocusTarget { FOCUS_CHAR_ROW, FOCUS_RESULT_LIST };
	FocusTarget mFocus;
	bool mLastInputWasKeyboard;
	int  mKeyRepeatKey;       // SDLK key held for repeat (backspace/delete/arrows), or 0
	int  mKeyRepeatTimer;
	int  mShoulderRepeatDir;  // gamepad shoulder cursor repeat: -1 / 0 / +1
	int  mShoulderRepeatTimer;
	static const int KEY_REPEAT_DELAY_MS  = 500;
	static const int KEY_REPEAT_PERIOD_MS = 80;
	unsigned int mResultListSelectorColor;
	unsigned int mResultListSelectorColorEnd;
};

#endif // ES_APP_GUIS_GUI_SEARCH_POPUP_H
