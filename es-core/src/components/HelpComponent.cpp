#include "components/HelpComponent.h"

#include "components/ComponentGrid.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "Settings.h"
#include "InputManager.h"

#define OFFSET_X 12 // move the entire thing right by this amount (px)
#define OFFSET_Y 12 // move the entire thing up by this amount (px)

#define ICON_TEXT_SPACING 8 // space between [icon] and [text] (px)
#define ENTRY_SPACING 16 // space between [text] and next [icon] (px)

static const HelpComponent::IconPathMap DEFAULT_ICON_PATH_MAP {
	{ "up/down", ":/help/dpad_updown.svg" },
	{ "left/right", ":/help/dpad_leftright.svg" },
	{ "up/down/left/right", ":/help/dpad_all.svg" },
	{ "a", ":/help/button_a.svg" },
	{ "b", ":/help/button_b.svg" },
	{ "x", ":/help/button_x.svg" },
	{ "y", ":/help/button_y.svg" },
	{ "l", ":/help/button_l.svg" },
	{ "r", ":/help/button_r.svg" },
	{ "lr", ":/help/button_lr.svg" },
	{ "start", ":/help/button_start.svg" },
	{ "select", ":/help/button_select.svg" }
};

static const HelpComponent::IconPathMap NO_ICON_OVERRIDES {};
static const HelpComponent::IconPathMap XBOX_ICON_OVERRIDES {
	{ "a", ":/help/button_b.svg" },
	{ "b", ":/help/button_a.svg" },
	{ "x", ":/help/button_y.svg" },
	{ "y", ":/help/button_x.svg" },
};
static const HelpComponent::IconPathMap PLAYSTATION_ICON_OVERRIDES {
	{ "a", ":/help/button_circle.svg" },
	{ "b", ":/help/button_cross.svg" },
	{ "x", ":/help/button_triangle.svg" },
	{ "y", ":/help/button_square.svg" },
};

HelpComponent::HelpComponent(Window* window) : GuiComponent(window)
{
}

void HelpComponent::clearPrompts()
{
	mPrompts.clear();
	updateGrid();
}

void HelpComponent::setPrompts(const std::vector<HelpPrompt>& prompts)
{
	mPrompts = prompts;
	updateGrid();
}

void HelpComponent::setStyle(const HelpStyle& style)
{
	mStyle = style;
	updateGrid();
}

void HelpComponent::updateGrid()
{
	if(!Settings::getInstance()->getBool("ShowHelpPrompts") || mPrompts.empty())
	{
		mGrid.reset();
		return;
	}

	std::shared_ptr<Font>& font = mStyle.font;

	mGrid = std::make_shared<ComponentGrid>(mWindow, Vector2i((int)mPrompts.size() * 4, 1));
	// [icon] [spacer1] [text] [spacer2]

	std::vector< std::shared_ptr<ImageComponent> > icons;
	std::vector< std::shared_ptr<TextComponent> > labels;

	const auto& iconOverrides =
		getIconOverridesForInput(InputManager::getInstance()->getInputConfigForLastUsedDevice());

	float width = 0;
	const float height = Math::round(font->getLetterHeight() * 1.25f);
	for(auto it = mPrompts.cbegin(); it != mPrompts.cend(); it++)
	{
		auto icon = std::make_shared<ImageComponent>(mWindow);
		icon->setImage(getIconTexture(it->first, iconOverrides));
		icon->setColorShift(mStyle.iconColor);
		icon->setResize(0, height);
		icons.push_back(icon);

		auto lbl = std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(it->second), font, mStyle.textColor);
		labels.push_back(lbl);

		width += icon->getSize().x() + lbl->getSize().x() + ICON_TEXT_SPACING + ENTRY_SPACING;
	}

	mGrid->setSize(width, height);
	for(unsigned int i = 0; i < icons.size(); i++)
	{
		const int col = i*4;
		mGrid->setColWidthPerc(col, icons.at(i)->getSize().x() / width);
		mGrid->setColWidthPerc(col + 1, ICON_TEXT_SPACING / width);
		mGrid->setColWidthPerc(col + 2, labels.at(i)->getSize().x() / width);

		mGrid->setEntry(icons.at(i), Vector2i(col, 0), false, false);
		mGrid->setEntry(labels.at(i), Vector2i(col + 2, 0), false, false);
	}

	mGrid->setPosition(Vector3f(mStyle.position.x(), mStyle.position.y(), 0.0f));
	//mGrid->setPosition(OFFSET_X, Renderer::getScreenHeight() - mGrid->getSize().y() - OFFSET_Y);
	mGrid->setOrigin(mStyle.origin);
}

const HelpComponent::IconPathMap& HelpComponent::getIconOverridesForInput(InputConfig* inputConfig)
{
	if(!inputConfig)
		return NO_ICON_OVERRIDES;

	switch(inputConfig->getButtonLayout())
	{
		case BUTTON_LAYOUT_PLAYSTATION:
			return PLAYSTATION_ICON_OVERRIDES;

		case BUTTON_LAYOUT_XBOX:
			return XBOX_ICON_OVERRIDES;

		case BUTTON_LAYOUT_DEFAULT:
			break;
	}

	return NO_ICON_OVERRIDES;
}

std::shared_ptr<TextureResource> HelpComponent::getIconTexture(const std::string& name, const IconPathMap& iconOverrides)
{
	auto pathLookup = iconOverrides.find(name);
	if(pathLookup == iconOverrides.cend())
	{
		pathLookup = DEFAULT_ICON_PATH_MAP.find(name);
		if(pathLookup == DEFAULT_ICON_PATH_MAP.cend())
		{
			LOG(LogError) << "Unknown help icon \"" << name << "\"!";
			return nullptr;
		}
	}

	auto it = mIconCache.find(pathLookup->second);
	if(it != mIconCache.cend())
		return it->second;

	if(!ResourceManager::getInstance()->fileExists(pathLookup->second))
	{
		LOG(LogError) << "Help icon \"" << name << "\" - corresponding image file \"" << pathLookup->second << "\" missing!";
		return nullptr;
	}

	std::shared_ptr<TextureResource> tex = TextureResource::get(pathLookup->second);
	mIconCache[pathLookup->second] = tex;
	return tex;
}

void HelpComponent::setOpacity(unsigned char opacity)
{
	GuiComponent::setOpacity(opacity);

	for(unsigned int i = 0; i < mGrid->getChildCount(); i++)
	{
		mGrid->getChild(i)->setOpacity(opacity);
	}
}

void HelpComponent::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	if(mGrid)
		mGrid->render(trans);
}
