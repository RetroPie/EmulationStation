#pragma once

#include <string>
#include <map>

class LocaleES
{
public:
    // Singleton
    static LocaleES& getInstance();

    // Carga archivo .ini desde una ruta
    bool loadLanguageFile(const std::string& filePath);

    // Carga el idioma según Settings ("Language")
    void loadFromSettings();

    // Traduce una clave usando la instancia global
    static std::string get(const std::string& key);

    // Traduce una clave usando esta instancia
    std::string translate(const std::string& key) const;

private:
    LocaleES(); // constructor privado (singleton)

    // Evitar copias
    LocaleES(const LocaleES&) = delete;
    LocaleES& operator=(const LocaleES&) = delete;

    std::string mCurrentLanguage;
    std::map<std::string, std::string> mTranslations;
};

// Función accesible desde cualquier parte (incluido es-core)
std::string es_translate(const std::string& key);
