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

std::filesystem::path g_resourcesDir = "resources";
std::string g_currentLang = "ru";
bool g_isExplicitLang = false;
std::vector<std::string> g_availableLanguages = {"ru", "en"};
std::unordered_map<std::string, std::string> g_strings;

std::filesystem::path getExecutableDir () {
  wchar_t buffer[MAX_PATH];
  DWORD length = GetModuleFileNameW (NULL, buffer, MAX_PATH);
  if (length > 0 && length < MAX_PATH) {
    return std::filesystem::path (buffer).parent_path ();
  }
  return {};
}

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

bool loadLanguageStrings (const std::filesystem::path &resDir, const std::string &langCode) {
  std::filesystem::path stringsFile = resDir / ("strings_" + langCode + ".json");
  std::ifstream in (stringsFile);
  if (!in.is_open ()) {
    return false;
  }

  try {
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

std::string detectKeyboardLanguage () {
  // Получаем раскладку активного окна пользователя (консоли / терминала)
  HWND fgWnd = GetForegroundWindow ();
  DWORD threadId = fgWnd ? GetWindowThreadProcessId (fgWnd, NULL) : 0;
  HKL hkl = threadId ? GetKeyboardLayout (threadId) : NULL;
  if (!hkl) {
    hkl = GetKeyboardLayout (0);
  }

  if (hkl) {
    WORD langId = LOWORD (reinterpret_cast<DWORD_PTR> (hkl));
    WORD primaryLang = PRIMARYLANGID (langId);
    if (primaryLang == LANG_RUSSIAN) {
      return "ru";
    }
  }

  // Все что не рус — английский
  return "en";
}

bool isExplicitLanguage () {
  return g_isExplicitLang;
}

bool init (const std::string &resourcesDir, const std::string &preferredLang) {
  g_resourcesDir = resolveResourcesPath (resourcesDir);
  g_isExplicitLang = !preferredLang.empty ();

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

  std::string targetLang;
  if (g_isExplicitLang) {
    targetLang = preferredLang;
  } else {
    // Автоматическое определение по раскладке клавиатуры: рус -> ru, все остальное -> en
    targetLang = detectKeyboardLanguage ();
  }

  return setLanguage (targetLang);
}

bool setLanguage (const std::string &langCode) {
  g_currentLang = langCode;
  bool loaded = loadLanguageStrings (g_resourcesDir, langCode);
  if (!loaded && langCode != "ru") {
    // Если запрошенный язык не найден, пробуем загрузить ru
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

  if (!defaultVal.empty ()) {
    return std::string (defaultVal);
  }

  return keyStr;
}

} // namespace loc
