#include "file_sync.h"
#include "localization.h"
#include "menu.h"

#include <memory>
#include <Windows.h>
#include <iostream>
#include <string>

/// @brief Параметры командной строки
struct CliOptions {
  std::string filePath;
  int mode {-1}; // 1: частота слов, 2: количество встреч и позиций, 0: полный отчет
  std::string resultPath {"result.txt"}; // имя файла результата по умолчанию
  std::string lang; // язык интерфейса (например "ru" или "en")
  bool hasMode {false};
  bool hasResult {false};
};

/// @brief Разбор аргументов командной строки (--mode, --file, --result)
CliOptions parseCommandLine (int argc, char *argv[]) {
  CliOptions opts;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--file" || arg == "-f") {
      if (i + 1 < argc) {
        opts.filePath = argv[++i];
      }
    } else if (arg == "--mode" || arg == "-m") {
      if (i + 1 < argc) {
        try {
          opts.mode = std::stoi (argv[++i]);
          opts.hasMode = true;
        } catch (...) {}
      }
    } else if (arg == "--result" || arg == "-r") {
      if (i + 1 < argc) {
        opts.resultPath = argv[++i];
        opts.hasResult = true;
      }
    } else if (arg == "--lang" || arg == "-l") {
      if (i + 1 < argc) {
        opts.lang = argv[++i];
      }
    } else if (arg == "--" && i + 1 < argc && std::string (argv[i + 1]) == "result") {
      // Поддержка варианта с пробелом: -- result "имя_файла"
      ++i;
      if (i + 1 < argc) {
        opts.resultPath = argv[++i];
        opts.hasResult = true;
      }
    } else if (arg.rfind ("--file=", 0) == 0) {
      opts.filePath = arg.substr (7);
    } else if (arg.rfind ("--mode=", 0) == 0) {
      try {
        opts.mode = std::stoi (arg.substr (7));
        opts.hasMode = true;
      } catch (...) {}
    } else if (arg.rfind ("--result=", 0) == 0) {
      opts.resultPath = arg.substr (9);
      opts.hasResult = true;
    } else if (arg.rfind ("--lang=", 0) == 0) {
      opts.lang = arg.substr (7);
    } else if (!opts.hasMode && (arg == "0" || arg == "1" || arg == "2")) {
      opts.mode = std::stoi (arg);
      opts.hasMode = true;
    } else if (opts.filePath.empty () && arg[0] != '-') {
      opts.filePath = arg;
    }
  }

  return opts;
}

int main (int argc, char *argv[]) {
  // Установка кодировки UTF-8 для корректного отображения символов
  SetConsoleCP (65001);
  SetConsoleOutputCP (65001);
  std::ios::sync_with_stdio (false);

  CliOptions opts = parseCommandLine (argc, argv);

  // Инициализация локализации из ресурсов
  loc::init ("resources", opts.lang);

  // Регистрируем сессию межпроцессной синхронизации для выходного файла,
  // чтобы разделяемая память оставалась активной на протяжении всей работы параллельных процессов
  std::unique_ptr<sync::FileSynchronizer> syncSession;
  if (opts.hasMode || opts.hasResult) {
    syncSession = std::make_unique<sync::FileSynchronizer> (opts.resultPath);
  }

  // 1. Если путь к файлу не был передан в аргументах, вызываем окно выбора
  if (opts.filePath.empty ()) {
    opts.filePath = app::OpenFileDialog ();
    if (opts.filePath.empty ()) {
      std::cout << loc::tr ("main.file_not_selected") << "\n";
      return 0;
    }
  }

  // 2. Загружаем и анализируем документ
  app::DocumentMenu menu (opts.filePath);
  if (!menu.isLoaded ()) {
    std::cout << loc::tr ("main.load_failed") << opts.filePath << "\n";
    return 1;
  }

  // 3. Если передан режим работы через аргументы (--mode 1, 2 или 0)
  if (opts.hasMode) {
    if (menu.saveModeReport (opts.mode, opts.resultPath, 10)) {
      std::cout << loc::tr ("main.result_saved") << opts.resultPath << "\n";
      return 0;
    } else {
      std::cerr << loc::tr ("main.result_error") << opts.resultPath << "\n";
      return 1;
    }
  }

  // 4. Если режим не был передан в аргументах — запускаем интерактивное меню
  menu.runInteractiveMenu ();

  return 0;
}
