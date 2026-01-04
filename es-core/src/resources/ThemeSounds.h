#pragma once

// Declaración adelantada para evitar dependencias circulares
class ThemeData;

// Identificadores de sonidos del tema
enum ThemeSoundId
{
    THEME_SOUND_SCROLL = 0,
    THEME_SOUND_SELECT = 1,
    THEME_SOUND_BACK   = 2
};

// Implementación mínima para que TODO compile sin romper nada.
// Más adelante se puede completar con carga real de sonidos.
class ThemeSounds
{
public:
    // Cargar sonidos desde el theme (por ahora no hace nada)
    static void loadFrom(const ThemeData& /*theme*/)
    {
        // TODO: Implementar lógica real de lectura de sonidos desde ThemeData
    }

    // Reproducir un sonido del theme (por ahora no hace nada)
    static void play(ThemeSoundId /*id*/)
    {
        // TODO: Implementar lógica real de reproducción más adelante
    }
};
