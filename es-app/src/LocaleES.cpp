#include "LocaleES.h"
#include "Settings.h"
#include "utils/FileSystemUtil.h"
#include "Log.h"

#include <fstream>
#include <sstream>
#include <algorithm>

// --------------------
// Singleton
// --------------------
LocaleES& LocaleES::getInstance()
{
    static LocaleES instance;
    return instance;
}

LocaleES::LocaleES()
    : mCurrentLanguage("en")
{
}

// --------------------
// Lectura de archivos .ini
// --------------------
bool LocaleES::loadLanguageFile(const std::string& filePath)
{
    std::ifstream in(filePath);
    if (!in.is_open())
    {
        LOG(LogWarning) << "LocaleES: could not open language file: " << filePath;
        return false;
    }

    mTranslations.clear();

    std::string line;

    auto trim = [](std::string& s)
    {
        // recorta espacios al inicio y al final
        size_t start = 0;
        while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n'))
            ++start;

        size_t end = s.size();
        while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n'))
            --end;

        s = s.substr(start, end - start);
    };

    while (std::getline(in, line))
    {
        trim(line);

        // Saltar líneas vacías, comentarios o secciones [XXX]
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[')
            continue;

        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key   = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        trim(key);
        trim(value);

        if (!key.empty())
            mTranslations[key] = value;
    }

    LOG(LogInfo) << "LocaleES: loaded " << mTranslations.size()
                 << " entries from " << filePath;
    return true;
}

// --------------------
// Cargar según Settings
// --------------------
void LocaleES::loadFromSettings()
{
    std::string lang = Settings::getInstance()->getString("Language");
    if (lang.empty())
        lang = "en";

    // Si ya está cargado este idioma y hay traducciones, no recargar
    if (lang == mCurrentLanguage && !mTranslations.empty())
        return;

    mCurrentLanguage = lang;
    mTranslations.clear();

    // Ruta en el home del usuario: ~/.emulationstation/lang/<CODE>.ini
    std::string home = Utils::FileSystem::getHomePath();
    std::string userPath = home + "/.emulationstation/lang/" + lang + ".ini";

    // Ruta opcional junto al ejecutable: <carpeta de ES>/lang/<CODE>.ini
    std::string exePath = Utils::FileSystem::getExePath();
    std::string appPath = exePath + "/lang/" + lang + ".ini";

    if (!loadLanguageFile(userPath))
    {
        if (!loadLanguageFile(appPath))
        {
            LOG(LogWarning) << "LocaleES: no language file found for '" << lang << "'";
        }
    }
}

// --------------------
// Traducciones
// --------------------
std::string LocaleES::translate(const std::string& key) const
{
    auto it = mTranslations.find(key);
    if (it != mTranslations.end())
        return it->second;

    // Si no hay traducción, devolvemos la clave tal cual
    return key;
}

std::string LocaleES::get(const std::string& key)
{
    return getInstance().translate(key);
}

// Función global para usar desde es-core
std::string es_translate(const std::string& key)
{
    return LocaleES::get(key);
}
