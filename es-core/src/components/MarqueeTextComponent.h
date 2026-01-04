#pragma once

#include "components/TextComponent.h"

// Componente de texto con scroll horizontal sencillo.
// No rompe compatibilidad con temas existentes y usa la misma API básica de TextComponent.
//
// NOTA: Esta versión no intenta calcular el ancho real del texto ni tocar mTextCache.
//       Solo desplaza el texto hacia la izquierda dentro del área del componente,
//       con un delay inicial y un bucle simple.
//

class MarqueeTextComponent : public TextComponent
{
public:
	MarqueeTextComponent(
		Window* window,
		const std::string& text,
		const std::shared_ptr<Font>& font,
		unsigned int color,
		Alignment align = ALIGN_LEFT);

	// Redefinimos setText solo para reiniciar el estado interno del scroll.
	// No usamos 'override' porque en tu TextComponent setText NO es virtual.
	void setText(const std::string& text);

	// Activar / desactivar scroll
	void setScrollEnabled(bool enabled) { mScrollEnabled = enabled; }
	bool getScrollEnabled() const { return mScrollEnabled; }

	// Velocidad en píxeles por segundo (por defecto 40)
	void setScrollSpeed(float speed) { mScrollSpeed = speed; }
	float getScrollSpeed() const { return mScrollSpeed; }

	// Retraso antes de empezar a moverse (en ms)
	void setScrollDelay(int delayMs) { mScrollDelayMs = delayMs; }
	int getScrollDelay() const { return mScrollDelayMs; }

	// Espacio "virtual" después del texto antes de reiniciar (en píxeles)
	void setScrollGap(float gap) { mScrollGap = gap; }
	float getScrollGap() const { return mScrollGap; }

	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

private:
	bool  mScrollEnabled;   // ¿scroll activado?
	bool  mScrollingActive; // ¿estamos desplazando ahora?

	float mScrollSpeed;     // px/seg
	float mScrollGap;       // gap fijo antes de reiniciar
	float mScrollOffset;    // desplazamiento actual en px

	int   mScrollDelayMs;   // delay inicial
	int   mElapsedMs;       // tiempo acumulado desde que se mostró
};
