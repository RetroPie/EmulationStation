#include "components/CharacterRowComponent.h"

#include "renderers/Renderer.h"
#include "InputConfig.h"

const std::string CharacterRowComponent::MODE_SWITCH_123     = "123";
const std::string CharacterRowComponent::MODE_SWITCH_ABC     = "ABC";
const std::string CharacterRowComponent::MODE_SWITCH_SYMBOLS = "!@#";
const std::string CharacterRowComponent::CHAR_SPACE          = "SPC";
const std::string CharacterRowComponent::CHAR_BACKSPACE      = "\xe2\x8c\xab"; // ⌫ U+232B
const std::string CharacterRowComponent::CHAR_CURSOR_LEFT    = "\xe2\x86\x90"; // ← U+2190
const std::string CharacterRowComponent::CHAR_CURSOR_RIGHT   = "\xe2\x86\x92"; // → U+2192

CharacterRowComponent::CharacterRowComponent(Window* window)
	: GuiComponent(window), mMode(LETTERS), mCursor(2), mFocused(true),
	  mSelectorColor(0x000050FF), mTextColor(0xFFFFFFFF), mTotalWidth(0),
	  mScrollDir(0), mScrollTimer(0), mBackspaceHeld(false), mBackspaceTimer(0)
{
	mFont = Font::get(FONT_SIZE_MEDIUM);
	buildCharList();
}

void CharacterRowComponent::buildCharList()
{
	mChars.clear();

	switch (mMode)
	{
	case LETTERS:
		mChars.push_back(MODE_SWITCH_123);
		mChars.push_back(CHAR_SPACE);
		for (char c = 'A'; c <= 'Z'; c++)
			mChars.push_back(std::string(1, c));
		break;

	case NUMBERS:
		mChars.push_back(MODE_SWITCH_SYMBOLS);
		mChars.push_back(CHAR_SPACE);
		for (char c = '1'; c <= '9'; c++)
			mChars.push_back(std::string(1, c));
		mChars.push_back("0");
		break;

	case SYMBOLS:
		mChars.push_back(MODE_SWITCH_ABC);
		mChars.push_back(CHAR_SPACE);
		{
			const char* syms = "`'\";:~=*+-_,.?!@#$%^&|/\\()[]{}<>";
			for (int i = 0; syms[i]; i++)
				mChars.push_back(std::string(1, syms[i]));
		}
		break;
	}

	mChars.push_back(CHAR_BACKSPACE);
	mChars.push_back(CHAR_CURSOR_LEFT);
	mChars.push_back(CHAR_CURSOR_RIGHT);

	if (mCursor >= (int)mChars.size())
		mCursor = (int)mChars.size() - 1;

	// Cache per-char widths — rebuilt when chars, font, or size changes
	// Compute natural padding, then shrink it if all chars don't fit in mSize.x()
	float padding = Math::round(mSize.y() * 0.2f);

	// First pass: measure raw text widths
	std::vector<float> textWidths;
	float totalTextWidth = 0;
	for (const auto& ch : mChars)
	{
		float tw = mFont->sizeText(ch).x();
		textWidths.push_back(tw);
		totalTextWidth += tw;
	}

	// Shrink padding if needed so everything fits
	if (mSize.x() > 0)
	{
		float maxPadding = (mSize.x() - totalTextWidth) / (2.0f * (float)mChars.size());
		if (maxPadding < padding)
			padding = std::max(1.0f, maxPadding);
	}

	mCharWidths.clear();
	mTotalWidth = 0;
	for (float tw : textWidths)
	{
		float w = tw + padding * 2;
		mCharWidths.push_back(w);
		mTotalWidth += w;
	}
}

void CharacterRowComponent::scrollStep(int dir)
{
	if (dir < 0)
	{
		if (mCursor > 0) mCursor--;
		else             mCursor = (int)mChars.size() - 1;
	}
	else
	{
		if (mCursor < (int)mChars.size() - 1) mCursor++;
		else                                   mCursor = 0;
	}
}

void CharacterRowComponent::update(int deltaTime)
{
	if (mScrollDir != 0)
	{
		mScrollTimer += deltaTime;
		while (mScrollTimer >= SCROLL_DELAY_MS)
		{
			mScrollTimer -= SCROLL_REPEAT_MS;
			scrollStep(mScrollDir);
		}
	}
	if (mBackspaceHeld)
	{
		mBackspaceTimer += deltaTime;
		while (mBackspaceTimer >= SCROLL_DELAY_MS)
		{
			mBackspaceTimer -= SCROLL_REPEAT_MS;
			if (mBackspaceCb) mBackspaceCb();
		}
	}
	GuiComponent::update(deltaTime);
}

bool CharacterRowComponent::input(InputConfig* config, Input input)
{
	if (input.value != 0)
	{
		if (config->isMappedLike("left", input))
		{
			mScrollDir   = -1;
			mScrollTimer = 0;
			scrollStep(-1);
			return true;
		}
		else if (config->isMappedLike("right", input))
		{
			mScrollDir   = 1;
			mScrollTimer = 0;
			scrollStep(1);
			return true;
		}
		else if (config->isMappedTo("x", input))
		{
			mBackspaceHeld = true;
			mBackspaceTimer = 0;
			if (mBackspaceCb) mBackspaceCb();
			return true;
		}
		else if (config->isMappedTo("a", input))
		{
			const std::string& selected = mChars[mCursor];

			if (selected == MODE_SWITCH_123)
			{
				mMode = NUMBERS;
				buildCharList();
			}
			else if (selected == MODE_SWITCH_SYMBOLS)
			{
				mMode = SYMBOLS;
				buildCharList();
			}
			else if (selected == MODE_SWITCH_ABC)
			{
				mMode = LETTERS;
				buildCharList();
			}
			else if (selected == CHAR_BACKSPACE)
			{
				if (mBackspaceCb)
					mBackspaceCb();
			}
			else if (selected == CHAR_CURSOR_LEFT)
			{
				if (mCursorLeftCb)
					mCursorLeftCb();
			}
			else if (selected == CHAR_CURSOR_RIGHT)
			{
				if (mCursorRightCb)
					mCursorRightCb();
			}
			else if (selected == CHAR_SPACE)
			{
				if (mCharSelectedCb)
					mCharSelectedCb(" ");
			}
			else
			{
				if (mCharSelectedCb)
					mCharSelectedCb(selected);
			}
			return true;
		}
	}

	if (input.value == 0)
	{
		if (mBackspaceHeld && config->isMappedTo("x", input))
		{
			mBackspaceHeld = false;
			return true;
		}
		if ((mScrollDir < 0 && config->isMappedLike("left", input)) ||
		    (mScrollDir > 0 && config->isMappedLike("right", input)))
		{
			mScrollDir = 0;
			return true;
		}
	}

	return GuiComponent::input(config, input);
}

void CharacterRowComponent::onSizeChanged()
{
	buildCharList();
}

void CharacterRowComponent::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	if (mChars.empty())
		return;

	const float height = mSize.y();

	// Rebuild cache if invalidated (shouldn't normally happen at render time)
	if (mCharWidths.size() != mChars.size())
		buildCharList();

	float startX = Math::round((mSize.x() - mTotalWidth) / 2.0f);
	if (startX < 0)
		startX = 0;

	float x = startX;
	const float textY = Math::round((height - mFont->getHeight(1.0f)) / 2.0f);

	Renderer::setMatrix(trans);

	for (int i = 0; i < (int)mChars.size(); i++)
	{
		float cellWidth = mCharWidths[i];
		float cellPadding = Math::round(cellWidth - mFont->sizeText(mChars[i]).x()) / 2.0f;

		if (mFocused && i == mCursor)
		{
			Renderer::drawRect(Math::round(x), 0, Math::round(cellWidth), Math::round(height),
				mSelectorColor, mSelectorColor);
		}

		auto textCache = std::unique_ptr<TextCache>(
			mFont->buildTextCache(mChars[i], Math::round(x + cellPadding), Math::round(textY),
				(i == mCursor) ? 0xFFFFFFFF : mTextColor));
		mFont->renderTextCache(textCache.get());

		x += cellWidth;
	}

	renderChildren(trans);
}
