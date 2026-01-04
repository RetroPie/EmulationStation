#pragma once

#include <map>
#include <string>

// Gestor simple de opciones por tema, basado en un INI:
// [Pi-Station-X]
// LAYOUT=ps4
// AVATAR=avatar03
// START_LABEL=INICIO
class ThemeOptions
{
public:
    // Singleton
    static ThemeOptions& getInstance();

    // Carga desde un INI. Si path == "" usa:
    // ~/.emulationstation/theme-options.ini
    void load(const std::string& path = "");

    // Guarda el INI en el mismo path usado en load()
    // o en el path por defecto si no se cargó ninguno.
    void save();

    // Lee una opción: sección = nombre del theme.
    // Si no existe, devuelve def.
    std::string get(const std::string& themeName,
                    const std::string& key,
                    const std::string& def = "") const;

    // Escribe/actualiza una opción en memoria.
    // NO guarda en disco hasta que se llame a save().
    void set(const std::string& themeName,
             const std::string& key,
             const std::string& value);

    const std::string& getPath() const { return mPath; }

private:
    ThemeOptions();
    ThemeOptions(const ThemeOptions&)            = delete;
    ThemeOptions& operator=(const ThemeOptions&) = delete;

    void clear();
    void parseLine(const std::string& line, std::string& currentSection);

    std::string mPath;
    // mData[section][key] = value
    std::map<std::string, std::map<std::string, std::string>> mData;
};
