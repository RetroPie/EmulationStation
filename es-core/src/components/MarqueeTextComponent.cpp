#include "components/MarqueeTextComponent.h"

#include "renderers/Renderer.h"
#include "Window.h"

MarqueeTextComponent::MarqueeTextComponent(
	Window* window,
	const std::string& text,
	const std::shared_ptr<Font>& font,
	unsigned int color,
	Alignment align)
	: TextComponent(window, text, font, color, align),
	  mScrollEnabled(true),
	  mScrollingActive(false),
	  mScrollSpeed(40.0f),   // velocidad base
	  mScrollGap(40.0f),     // gap al final
	  mScrollOffset(0.0f),
	  mScrollDelayMs(1200),  // 1.2s de espera inicial
	  mElapsedMs(0)
{
}

void MarqueeTextComponent::setText(const std::string& text)
{
	// llamamos al TextComponent original
	TextComponent::setText(text);

	// reiniciamos el estado del scroll
	mScrollOffset    = 0.0f;
	mElapsedMs       = 0;
	mScrollingActive = false;
}

void MarqueeTextComponent::update(int deltaTime)
{
	// Primero dejamos que TextComponent haga lo suyo (cacheo, etc.)
	TextComponent::update(deltaTime);

	if (!mScrollEnabled)
		return;

	// Acumulamos tiempo
	mElapsedMs += deltaTime;

	// Activamos scroll después del delay inicial
	if (!mScrollingActive)
	{
		if (mElapsedMs >= mScrollDelayMs)
		{
			mScrollingActive = true;
			mElapsedMs       = 0;
		}
		else
		{
			return;
		}
	}

	// deltaTime viene en ms → pasamos a segundos
	const float dt = deltaTime / 1000.0f;
	mScrollOffset += mScrollSpeed * dt;

	// Usamos un ciclo simple basado en el ancho del componente + gap fijo
	const float boxWidth = getSize().x();
	const float resetPoint = boxWidth + mScrollGap;

	if (boxWidth > 0.0f && mScrollOffset > resetPoint)
	{
		mScrollOffset = 0.0f;
		mElapsedMs    = 0;
		mScrollingActive = false; // volvemos a aplicar delay antes del próximo ciclo
	}
}

void MarqueeTextComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	// Transform base del componente (sin scroll)
	Transform4x4f trans = parentTrans * getTransform();
	Vector3f      pos   = trans.translation();
	Vector2f      size  = getSize();

	// Definimos el área de recorte igual que otros componentes
	Renderer::pushClipRect(
		Vector2i((int)pos.x(), (int)pos.y()),
		Vector2i((int)size.x(), (int)size.y()));

	// Creamos un parentTrans con el offset de scroll aplicado
	Transform4x4f parentWithScroll = parentTrans;

	if (mScrollingActive)
	{
		// movemos el texto hacia la izquierda dentro del cuadro
		parentWithScroll.translate(Vector3f(-mScrollOffset, 0.0f, 0.0f));
	}

	// Dejamos que TextComponent haga el render normal,
	// pero usando el parentTrans desplazado
	TextComponent::render(parentWithScroll);

	Renderer::popClipRect();
}
