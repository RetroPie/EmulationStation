#include "ThemeSounds.h"
#include "ThemeData.h"

// De momento no hacemos nada aquí porque la implementación está en el header
// Solo dejamos las definiciones vacías para que el linker esté contento
void ThemeSounds::loadFrom(const ThemeData& /*theme*/)
{
    // En el futuro: leer rutas de sonidos desde ThemeData y guardarlas en estáticos
}

void ThemeSounds::play(ThemeSoundId /*id*/)
{
    // En el futuro: usar AudioManager/Sound para reproducir los sonidos cargados
}
