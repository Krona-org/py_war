#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace loc {

/// @brief Определение языка текущей активной раскладки клавиатуры (русская -> "ru", все остальное -> "en")
std::string detectKeyboardLanguage ();

/// @brief Проверка, был ли язык задан явно пользователем (например, через аргумент --lang)
bool isExplicitLanguage ();

/// @brief Инициализация системы локализации
/// @param resourcesDir Путь к папке ресурсов с JSON файлами (по умолчанию "resources")
/// @param preferredLang Желаемый язык (если пустой, определяется по текущей раскладке)
/// @return true, если ресурсы успешно загружены
bool init (const std::string &resourcesDir = "resources", const std::string &preferredLang = "");

/// @brief Смена активного языка
/// @param langCode Код языка (например, "ru" или "en")
/// @return true, если язык успешно установлен
bool setLanguage (const std::string &langCode);

/// @brief Получение текущего кода языка
const std::string &getLanguage ();

/// @brief Получение списка доступных языков из languages.json
const std::vector<std::string> &getAvailableLanguages ();

/// @brief Получение локализованной строки по ключу (аналог Qt tr())
/// @param key Ключ строки (например, "menu.title")
/// @param defaultVal Значение по умолчанию, если ключ не найден
/// @return Локализованная строка
std::string tr (std::string_view key, std::string_view defaultVal = "");

} // namespace loc
