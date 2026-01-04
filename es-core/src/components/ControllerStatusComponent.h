#pragma once

#include "GuiComponent.h"
#include <memory>
#include <string>

class Font;

class ControllerStatusComponent : public GuiComponent
{
public:
	explicit ControllerStatusComponent(Window* window);

	void setMaxControllers(int maxControllers);
	void setIconSize(float size); // relativo a altura (ej: 0.035f)

	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

private:
	std::shared_ptr<Font> mFont;
	int   mMaxControllers;
	float mIconSize;
};
