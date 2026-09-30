#include "localization.h"
#include <nlohmann/json.hpp>

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>

namespace loc {

namespace {

using json = nlohmann::json;

// Внутреннее состояние локализации
std::filesystem::path g_resourcesDir = "resources";             ///< Разрешенный относительный путь к каталогу ресурсов
std::string g_currentLang = "ru";                               ///< Текущий активный код языка ("ru", "en")
bool g_isExplicitLang = false;                                  ///< Задан ли язык явно через флаг командной строки (--lang)
std::vector<std::string> g_availableLanguages = {"ru", "en"};   ///< Доступные языки из languages.json
std::unordered_map<std::string, std::string> g_strings;         ///< Загруженная в память хэш-таблица строк текущего языка

/// @brief Получение пути к каталогу, содержащему исполняемый файл (.exe) процесса
/// Использует Win32 API GetModuleFileNameW для получения полного пути к запущенному модулю,
/// что позволяет приложению оставаться полностью независимым от текущего рабочего каталога (CWD).
/// @return Путь к папке с исполняемым файлом
std::filesystem::path getExecutableDir () {
  wchar_t buffer[MAX_PATH];
  DWORD length = GetModuleFileNameW (NULL, buffer, MAX_PATH);
  if (length > 0 && length < MAX_PATH) {
    return std::filesystem::path (buffer).parent_path ();
  }
  return {};
}

/// @brief Поиск папки с ресурсами по цепочке относительных путей
/// Исключает жесткую привязку к абсолютным путям диска разработчика.
/// Проверяет следующие места поиска:
/// 1. Непосредственно указанный путь (например, "./resources")
/// 2. Относительно текущего рабочего каталога
/// 3. На один уровень выше текущего каталога (если запуск из build_debug/)
/// 4. Рядом с исполняемым файлом .exe (exeDir / resources)
/// 5. На уровень выше исполняемого файла (exeDir / ../resources)
/// @param baseDir Базовое имя папки ресурсов
/// @return Найденный путь к каталогу ресурсов
std::filesystem::path resolveResourcesPath (const std::string &baseDir) {
  std::filesystem::path exeDir = getExecutableDir ();

  std::vector<std::filesystem::path> searchCandidates = {
      baseDir,
      std::filesystem::current_path () / baseDir,
      std::filesystem::current_path () / ".." / baseDir,
  };

  if (!exeDir.empty ()) {
    searchCandidates.push_back (exeDir / baseDir);
    searchCandidates.push_back (exeDir / ".." / baseDir);
  }

  for (const auto &candidate : searchCandidates) {
    if (std::filesystem::exists (candidate / "languages.json")) {
      return candidate;
    }
  }

  return baseDir;
}

/// @brief Загрузка строковых ресурсов из JSON-файла для заданного языка
/// Читает файл strings_<langCode>.json с помощью nlohmann::json и заполняет хэш-таблицу g_strings.
/// @param resDir Каталог с ресурсами
/// @param langCode Код языка ("ru", "en")
/// @return true, если файл успешно прочитан и распарсен
bool loadLanguageStrings (const std::filesystem::path &resDir, const std::string &langCode) {
  std::filesystem::path stringsFile = resDir / ("strings_" + langCode + ".json");
  std::ifstream in (stringsFile);
  if (!in.is_open ()) {
    return false;
  }

  try {
    // Парсим структуру JSON через nlohmann::json напрямую из файлового потока std::ifstream
    json data = json::parse (in);
    g_strings.clear ();
    for (auto it = data.begin (); it != data.end (); ++it) {
      if (it.value ().is_string ()) {
        g_strings[it.key ()] = it.value ().get<std::string> ();
      }
    }
    return true;
  } catch (...) {
    return false;
  }
}

} // namespace

/// @brief Автоматическое определение языка ввода по активной раскладке клавиатуры в Windows
/// Логика работы:
/// 1. GetForegroundWindow() находит окно переднего плана (терминал пользователя).
/// 2. GetWindowThreadProcessId() получает идентификатор потока этого окна.
/// 3. GetKeyboardLayout() возвращает дескриптор раскладки (HKL) активного потока ввода.
/// 4. Младшие 16 бит (LOWORD) дескриптора содержат Language Identifier (LANGID).
/// 5. Макрос PRIMARYLANGID отсекает региональные диалекты (оставляет базовый идентификатор языка).
/// 6. Если базовый язык равен LANG_RUSSIAN (0x19) -> возвращаем "ru", иначе -> "en".
/// @return Код языка: "ru" для русской раскладки, "en" для любой другой
std::string detectKeyboardLanguage () {
  HWND fgWnd = GetForegroundWindow ();
  DWORD threadId = fgWnd ? GetWindowThreadProcessId (fgWnd, NULL) : 0;
  HKL hkl = threadId ? GetKeyboardLayout (threadId) : NULL;
  if (!hkl) {
    // Резервный опрос текущего потока приложения
    hkl = GetKeyboardLayout (0);
  }

  if (hkl) {
    WORD langId = LOWORD (reinterpret_cast<DWORD_PTR> (hkl));
    WORD primaryLang = PRIMARYLANGID (langId);
    if (primaryLang == LANG_RUSSIAN) {
      return "ru";
    }
  }

  // Все нерусские раскладки по требованию мапятся в английский язык
  return "en";
}

bool isExplicitLanguage () {
  return g_isExplicitLang;
}

bool init (const std::string &resourcesDir, const std::string &preferredLang) {
  g_resourcesDir = resolveResourcesPath (resourcesDir);
  g_isExplicitLang = !preferredLang.empty ();

  // Загружаем список зарегистрированных языков из languages.json
  std::filesystem::path langConfigPath = g_resourcesDir / "languages.json";
  std::ifstream in (langConfigPath);

  if (in.is_open ()) {
    try {
      json data = json::parse (in);
      if (data.contains ("languages") && data["languages"].is_array ()) {
        g_availableLanguages.clear ();
        for (const auto &item : data["languages"]) {
          if (item.is_string ()) {
            g_availableLanguages.push_back (item.get<std::string> ());
          }
        }
      }
    } catch (...) {}
  }

  // Выбираем целевой язык: либо принудительный из командной строки,
  // либо автоматически определяемый по текущей раскладке клавиатуры
  std::string targetLang;
  if (g_isExplicitLang) {
    targetLang = preferredLang;
  } else {
    targetLang = detectKeyboardLanguage ();
  }

  return setLanguage (targetLang);
}

bool setLanguage (const std::string &langCode) {
  g_currentLang = langCode;
  bool loaded = loadLanguageStrings (g_resourcesDir, langCode);
  if (!loaded && langCode != "ru") {
    // Если запрошенный язык не найден, пробуем базовый русский
    loaded = loadLanguageStrings (g_resourcesDir, "ru");
  }
  return loaded;
}

const std::string &getLanguage () {
  return g_currentLang;
}

const std::vector<std::string> &getAvailableLanguages () {
  return g_availableLanguages;
}

std::string tr (std::string_view key, std::string_view defaultVal) {
  std::string keyStr (key);
  auto it = g_strings.find (keyStr);
  if (it != g_strings.end ()) {
    return it->second;
  }

  // Если ключ не найден в JSON-файле ресурсов, возвращаем дефолтное значение либо сам ключ
  if (!defaultVal.empty ()) {
    return std::string (defaultVal);
  }

  return keyStr;
}

} // namespace loc
