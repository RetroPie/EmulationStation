#include "components/ControllerStatusComponent.h"

#include "InputManager.h"
#include "renderers/Renderer.h"
#include "resources/Font.h"

ControllerStatusComponent::ControllerStatusComponent(Window* window)
	: GuiComponent(window),
	  mMaxControllers(4),
	  mIconSize(0.035f)
{
	// Fuente players.ttf dentro de resources
	mFont = Font::getFromFile(
		":/fonts/players.ttf",
		(int)(Renderer::getScreenHeight() * mIconSize),
		FONT_PATH_REGULAR);

	setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
}

void ControllerStatusComponent::setMaxControllers(int maxControllers)
{
	if (maxControllers < 1)
		maxControllers = 1;
	mMaxControllers = maxControllers;
}

void ControllerStatusComponent::setIconSize(float size)
{
	if (size < 0.01f)
		size = 0.01f;

	mIconSize = size;

	// Recarga tamaño de fuente
	mFont = Font::getFromFile(
		":/fonts/players.ttf",
		(int)(Renderer::getScreenHeight() * mIconSize),
		FONT_PATH_REGULAR);
}

void ControllerStatusComponent::update(int /*deltaTime*/)
{
	// UI pasiva: nada por ahora
}

void ControllerStatusComponent::render(const Transform4x4f& parentTrans)
{
	if (!mFont)
		return;

	// Batocera/ES-DE style: tomamos el mapa real de “players”
	// En tu log ya se ve que InputManager detecta joysticks.
	auto players = InputManager::getInstance()->lastKnownPlayersDeviceIndexes();

	int count = 0;
	for (auto& kv : players)
	{
		if (kv.second >= 0)
			count++;
	}

	if (count <= 0)
		return;

	if (count > mMaxControllers)
		count = mMaxControllers;

	Transform4x4f trans = parentTrans * getTransform();

	// Arriba a la derecha (ajustable después por tema si querés)
	const float y = Renderer::getScreenHeight() * 0.04f;

	// Render de “P1..Pn” como dígitos (players.ttf en Batocera suele mapear 1..4)
	// Vamos de derecha a izquierda para que quede pegado al borde.
	float x = Renderer::getScreenWidth() * 0.96f;

	for (int i = 0; i < count; i++)
	{
		const std::string glyph = std::to_string(i + 1);
		const auto size = mFont->sizeText(glyph);

		x -= size.x();
		mFont->renderText(glyph, x, y, trans, 0xFFFFFFFF);

		// pequeño espaciado
		x -= Renderer::getScreenWidth() * 0.006f;
	}
}
