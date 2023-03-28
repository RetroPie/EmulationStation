#pragma once
#ifndef ES_CORE_COMPONENTS_HELP_COMPONENT_H
#define ES_CORE_COMPONENTS_HELP_COMPONENT_H

#include "GuiComponent.h"
#include "HelpStyle.h"
#include "resources/TextureResource.h"

#include <string>

class ComponentGrid;
class ImageComponent;
class TextureResource;

class HelpComponent : public GuiComponent
{
public:
	HelpComponent(Window* window);

	void clearPrompts();
	void setPrompts(const std::vector<HelpPrompt>& prompts);

	void render(const Transform4x4f& parent) override;
	void setOpacity(unsigned char opacity) override;

	void setStyle(const HelpStyle& style);

	using IconPathMap = std::map<std::string /*name*/, std::string /*path*/>;

private:
	const IconPathMap& getIconOverridesForInput(InputConfig* inputConfig);
	std::shared_ptr<TextureResource> getIconTexture(const std::string& name, const IconPathMap& iconOverrides);
	std::map< std::string, std::shared_ptr<TextureResource> > mIconCache;

	std::shared_ptr<ComponentGrid> mGrid;
	void updateGrid();

	std::vector<HelpPrompt> mPrompts;
	HelpStyle mStyle;
};

#endif // ES_CORE_COMPONENTS_HELP_COMPONENT_H
