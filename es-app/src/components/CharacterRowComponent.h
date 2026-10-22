#pragma once
#ifndef ES_APP_COMPONENTS_CHARACTER_ROW_COMPONENT_H
#define ES_APP_COMPONENTS_CHARACTER_ROW_COMPONENT_H

#include "GuiComponent.h"
#include "resources/Font.h"
#include <functional>
#include <string>
#include <vector>

class CharacterRowComponent : public GuiComponent
{
public:
	CharacterRowComponent(Window* window);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void onSizeChanged() override;
	void render(const Transform4x4f& parentTrans) override;

	void setCharSelectedCallback(const std::function<void(const std::string&)>& cb) { mCharSelectedCb = cb; }
	void setBackspaceCallback(const std::function<void()>& cb) { mBackspaceCb = cb; }
	void setCursorLeftCallback(const std::function<void()>& cb) { mCursorLeftCb = cb; }
	void setCursorRightCallback(const std::function<void()>& cb) { mCursorRightCb = cb; }

	enum Mode { LETTERS, NUMBERS, SYMBOLS };
	Mode getMode() const { return mMode; }

	void setFont(const std::shared_ptr<Font>& font) { mFont = font; buildCharList(); }
	void setSelectorColor(unsigned int color) { mSelectorColor = color; }
	void setTextColor(unsigned int color) { mTextColor = color; }
	void setFocused(bool focused) { mFocused = focused; }

	int getCursor() const { return mCursor; }

private:
	void buildCharList();

	void scrollStep(int dir); // dir: -1 = left, +1 = right

	static const int SCROLL_DELAY_MS    = 500;
	static const int SCROLL_REPEAT_MS   = 100;

	Mode mMode;
	int mCursor;
	bool mFocused;
	int mScrollDir;    // -1, 0, or 1
	int mScrollTimer;
	bool mBackspaceHeld;
	int mBackspaceTimer;
	std::vector<std::string> mChars;

	std::function<void(const std::string&)> mCharSelectedCb;
	std::function<void()> mBackspaceCb;
	std::function<void()> mCursorLeftCb;
	std::function<void()> mCursorRightCb;

	std::shared_ptr<Font> mFont;
	unsigned int mSelectorColor;
	unsigned int mTextColor;

	std::vector<float> mCharWidths;
	float mTotalWidth;

	static const std::string MODE_SWITCH_123;
	static const std::string MODE_SWITCH_ABC;
	static const std::string MODE_SWITCH_SYMBOLS;
	static const std::string CHAR_SPACE;
	static const std::string CHAR_BACKSPACE;  // ⌫
	static const std::string CHAR_CURSOR_LEFT;
	static const std::string CHAR_CURSOR_RIGHT;
};

#endif // ES_APP_COMPONENTS_CHARACTER_ROW_COMPONENT_H
