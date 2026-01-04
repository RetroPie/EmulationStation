#include "components/HelpComponent.h"

#include "components/ComponentGrid.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "resources/TextureResource.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "Settings.h"
#include "../LocaleESHook.h"   // Para es_translate()

#define OFFSET_X 12 
#define OFFSET_Y 12 

#define ICON_TEXT_SPACING 8
#define ENTRY_SPACING 16

// 🔹 Mapa de íconos
static const std::map<std::string, const char*> ICON_PATH_MAP {
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

// 🔹 Normaliza claves como "favorites", "favs", "favorite" → "FAVORITES"
static std::string normalizeKey(const std::string& key)
{
	std::string k = Utils::String::toLower(key);

	if (k == "favorites" || k == "favs" || k == "favorite")
		return "FAVORITES";

	if (k == "back")
		return "BACK";

	if (k == "select")
		return "SELECT";

	if (k == "menu")
		return "MENU";

	if (k == "random")
		return "RANDOM";

	if (k == "choose")
		return "CHOOSE";

	return Utils::String::toUpper(key);
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

	std::vector< std::shared_ptr<ImageComponent> > icons;
	std::vector< std::shared_ptr<TextComponent> > labels;

	float width = 0;
	const float height = Math::round(font->getLetterHeight() * 1.25f);

	for(auto it = mPrompts.cbegin(); it != mPrompts.cend(); it++)
	{
		auto icon = std::make_shared<ImageComponent>(mWindow);
		icon->setImage(getIconTexture(it->first.c_str()));
		icon->setColorShift(mStyle.iconColor);
		icon->setResize(0, height);
		icons.push_back(icon);

		// 🔹 Traducción corregida
		std::string normalizedKey = normalizeKey(it->second);
		std::string translated = es_translate(normalizedKey);

		auto lbl = std::make_shared<TextComponent>(
			mWindow,
			Utils::String::toUpper(translated),
			font,
			mStyle.textColor
		);
		labels.push_back(lbl);

		width += icon->getSize().x() + lbl->getSize().x() + ICON_TEXT_SPACING + ENTRY_SPACING;
	}

	mGrid->setSize(width, height);

	for(unsigned int i = 0; i < icons.size(); i++)
	{
		const int col = i * 4;
		mGrid->setColWidthPerc(col, icons.at(i)->getSize().x() / width);
		mGrid->setColWidthPerc(col + 1, ICON_TEXT_SPACING / width);
		mGrid->setColWidthPerc(col + 2, labels.at(i)->getSize().x() / width);

		mGrid->setEntry(icons.at(i), Vector2i(col, 0), false, false);
		mGrid->setEntry(labels.at(i), Vector2i(col + 2, 0), false, false);
	}

	mGrid->setPosition(Vector3f(mStyle.position.x(), mStyle.position.y(), 0.0f));
}

std::shared_ptr<TextureResource> HelpComponent::getIconTexture(const char* name)
{
	auto it = mIconCache.find(name);
	if(it != mIconCache.cend())
		return it->second;

	auto pathLookup = ICON_PATH_MAP.find(name);
	if(pathLookup == ICON_PATH_MAP.cend())
	{
		LOG(LogError) << "Unknown help icon \"" << name << "\"!";
		return nullptr;
	}
	if(!ResourceManager::getInstance()->fileExists(pathLookup->second))
	{
		LOG(LogError) << "Missing icon file \"" << pathLookup->second << "\"!";
		return nullptr;
	}

	auto tex = TextureResource::get(pathLookup->second);
	mIconCache[std::string(name)] = tex;
	return tex;
}

void HelpComponent::setOpacity(unsigned char opacity)
{
	GuiComponent::setOpacity(opacity);

	for(unsigned int i = 0; i < mGrid->getChildCount(); i++)
		mGrid->getChild(i)->setOpacity(opacity);
}

void HelpComponent::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	if(mGrid)
		mGrid->render(trans);
}
