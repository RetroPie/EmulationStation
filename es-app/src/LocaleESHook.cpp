#include "LocaleES.h"

// Ruta relativa: ajusta si tu estructura es distinta
#include "../es-core/src/LocaleESHook.h"

std::string es_translate(const std::string& key)
{
    return LocaleES::getInstance().translate(key);
}
