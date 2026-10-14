#pragma once

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

inline std::string DetectCurrentLayoutLanguage () {
  HWND hwnd = GetForegroundWindow ();
  DWORD threadId = hwnd ? GetWindowThreadProcessId (hwnd, NULL) : 0;
  HKL hkl = GetKeyboardLayout (threadId ? threadId : 0);
  WORD langId = LOWORD (hkl);

  // 0x0419 — русский язык. Для всех остальных языков используется en
  if (langId == 0x0419) {
    return "ru";
  }
  return "en";
}

class LocalizationManager {
public:
  static LocalizationManager &getInstance () {
    static LocalizationManager instance;
    return instance;
  }

  void loadResources (const std::string &filename = "resources.txt") {
    // Проверяем несколько возможных путей: текущая директория, папка с exe, папка проекта
    std::vector<std::filesystem::path> searchPaths = {
        filename,
        std::filesystem::path ("..") / filename,
        std::filesystem::current_path () / filename
    };

    char exePathBuf[MAX_PATH] = {0};
    if (GetModuleFileNameA (NULL, exePathBuf, MAX_PATH)) {
      std::filesystem::path exeDir = std::filesystem::path (exePathBuf).parent_path ();
      searchPaths.push_back (exeDir / filename);
      searchPaths.push_back (exeDir.parent_path () / filename);
    }

    bool loaded = false;
    for (const auto &path : searchPaths) {
      if (std::filesystem::exists (path)) {
        if (parseFile (path)) {
          loaded = true;
          break;
        }
      }
    }

    if (!loaded) {
      loadDefaults ();
    }
  }

  std::string get (const std::string &key, const std::string &lang = "") const {
    std::string currentLang = lang.empty () ? DetectCurrentLayoutLanguage () : lang;

    auto langIt = resources_.find (currentLang);
    if (langIt != resources_.end ()) {
      auto valIt = langIt->second.find (key);
      if (valIt != langIt->second.end ()) {
        return valIt->second;
      }
    }

    // Резервный поиск в en
    auto fallbackIt = resources_.find ("en");
    if (fallbackIt != resources_.end ()) {
      auto valIt = fallbackIt->second.find (key);
      if (valIt != fallbackIt->second.end ()) {
        return valIt->second;
      }
    }

    return key;
  }

private:
  LocalizationManager () {
    loadDefaults ();
    loadResources ();
  }

  void loadDefaults () {
    resources_["ru"] = {
        {"MENU_TITLE", "==================== ГЛАВНОЕ МЕНЮ ===================="},
        {"MENU_OPT_1", "1. Подсчёт уникальных слов в файле"},
        {"MENU_OPT_2", "2. Индексация позиций слов в файле"},
        {"MENU_OPT_3", "3. STL 3a: Возведение простых чисел в квадрат"},
        {"MENU_OPT_4", "4. STL 3b: Сортировка нечётных (возр.) и чётных (убыв.)"},
        {"MENU_OPT_5", "5. STL 3c: Поиск уникальных чисел в диапазоне"},
        {"MENU_OPT_0", "0. Выход"},
        {"PROMPT_SELECT", "Выберите пункт меню: "},
        {"PROMPT_FILE", "Выберите входной файл через окно диалога..."},
        {"MSG_WAIT_LOCK", "Ожидание очереди записи в файл..."},
        {"MSG_SUCCESS", "Результат успешно записан в файл."}
    };

    resources_["en"] = {
        {"MENU_TITLE", "===================== MAIN MENU ====================="},
        {"MENU_OPT_1", "1. Count unique words in file"},
        {"MENU_OPT_2", "2. Index word positions in file"},
        {"MENU_OPT_3", "3. STL 3a: Square prime numbers"},
        {"MENU_OPT_4", "4. STL 3b: Sort odd (asc) and even (desc)"},
        {"MENU_OPT_5", "5. STL 3c: Find unique numbers in range"},
        {"MENU_OPT_0", "0. Exit"},
        {"PROMPT_SELECT", "Select menu option: "},
        {"PROMPT_FILE", "Selecting input file via OpenDialog..."},
        {"MSG_WAIT_LOCK", "Waiting in queue for file access..."},
        {"MSG_SUCCESS", "Result successfully appended to file."}
    };
  }

  static std::string trim (const std::string &str) {
    size_t first = str.find_first_not_of (" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of (" \t\r\n");
    return str.substr (first, (last - first + 1));
  }

  bool parseFile (const std::filesystem::path &filePath) {
    std::ifstream file (filePath);
    if (!file.is_open ()) return false;

    std::string line;
    std::string currentSection;

    while (std::getline (file, line)) {
      std::string trimmed = trim (line);
      if (trimmed.empty () || trimmed[0] == ';' || trimmed[0] == '#') continue;

      if (trimmed.front () == '[' && trimmed.back () == ']') {
        currentSection = trim (trimmed.substr (1, trimmed.size () - 2));
      } else if (!currentSection.empty ()) {
        size_t eqPos = trimmed.find ('=');
        if (eqPos != std::string::npos) {
          std::string key = trim (trimmed.substr (0, eqPos));
          std::string value = trim (trimmed.substr (eqPos + 1));
          resources_[currentSection][key] = value;
        }
      }
    }
    return true;
  }

  std::unordered_map<std::string, std::unordered_map<std::string, std::string>> resources_;
};
