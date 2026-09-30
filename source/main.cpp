#include "file_sync.h"
#include "localization.h"
#include "menu.h"

#include <Windows.h>
#include <iostream>
#include <memory>
#include <string>

/// @brief Параметры конфигурации приложения, передаваемые через командную строку
struct CliOptions {
  std::string filePath; ///< Путь к анализируемому текстовому файлу
  int mode {-1};        ///< Режим работы: 1 - частотный анализ, 2 - индексация позиций, 0 - полный отчет
  std::string resultPath {
      "result.txt"};      ///< Имя целевого файла для сохранения отчета (по умолчанию result.txt)
  std::string lang;       ///< Принудительный язык интерфейса ("ru", "en"). Если пуст - берется раскладка
  bool hasMode {false};   ///< Флаг: передан ли параметр режима работы
  bool hasResult {false}; ///< Флаг: передано ли пользовательское имя файла для результатов
};

/// @brief Разбор аргументов командной строки приложения
/// Поддерживает как стандартный формат флаг-значение (--file path, -f path),
/// так и синтаксис со знаком равенства (--file=path), а также короткие формы флагов.
/// @param argc Количество аргументов
/// @param argv Массив указателей на строки аргументов
/// @return Заполненная структура CliOptions
CliOptions parseCommandLine (int argc, char *argv[]) {
  CliOptions opts;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    // Путь к исходному документу
    if (arg == "--file" || arg == "-f") {
      if (i + 1 < argc) {
        opts.filePath = argv[++i];
      }
    } else if (arg == "--mode" || arg == "-m") {
      // Режим генерации отчета: 1 (частота слов), 2 (индексация позиций) или 0 (все вместе)
      if (i + 1 < argc) {
        try {
          opts.mode = std::stoi (argv[++i]);
          opts.hasMode = true;
        } catch (...) {
        }
      }
    } else if (arg == "--result" || arg == "-r") {
      // Путь к файлу с результатом обработки
      if (i + 1 < argc) {
        opts.resultPath = argv[++i];
        opts.hasResult = true;
      }
    } else if (arg == "--lang" || arg == "-l") {
      // Принудительный выбор языка интерфейса ("ru" или "en")
      if (i + 1 < argc) {
        opts.lang = argv[++i];
      }
    } else if (arg == "--" && i + 1 < argc && std::string (argv[i + 1]) == "result") {
      // Поддержка редкого формата записи с разделяющим пробелом: -- result "имя_файла"
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
      } catch (...) {
      }
    } else if (arg.rfind ("--result=", 0) == 0) {
      opts.resultPath = arg.substr (9);
      opts.hasResult = true;
    } else if (arg.rfind ("--lang=", 0) == 0) {
      opts.lang = arg.substr (7);
    } else if (!opts.hasMode && (arg == "0" || arg == "1" || arg == "2")) {
      // Упрощенный ввод режима одной цифрой без префикса (например: lab1.exe 1 file.txt)
      opts.mode = std::stoi (arg);
      opts.hasMode = true;
    } else if (opts.filePath.empty () && arg[0] != '-') {
      // Позиционный аргумент: первый параметр без дефиса трактуется как путь к файлу
      opts.filePath = arg;
    }
  }

  return opts;
}

int main (int argc, char *argv[]) {
  // Установка кодовой страницы консоли в UTF-8 (CP 65001) для корректного отображения кириллицы и
  // спецсимволов
  SetConsoleCP (65001);
  SetConsoleOutputCP (65001);
  std::ios::sync_with_stdio (false);

  CliOptions opts = parseCommandLine (argc, argv);

  // Инициализация модуля интернационализации:
  // Если opts.lang пустой, модуль автоматически опросит активную раскладку клавиатуры Windows
  // (GetKeyboardLayout). Если раскладка русская -> "ru", иначе -> "en". Все строки читаются из
  // resources/strings_<lang>.json.
  loc::init ("resources", opts.lang);

  // Регистрация долгоживущей сессии межпроцессной синхронизации (IPC) в main():
  // В Windows объект именованной разделяемой памяти (FileMapping) уничтожается ядром, когда все дескрипторы
  // к нему закрыты. Удержание syncSession на протяжении жизни процесса гарантирует, что 5 одновременно
  // запущенных процессов будут видеть одну и ту же общую память и счетчик activeCount, что обеспечивает
  // правильное выстраивание процессов в очередь к файлу результатов.
  std::unique_ptr<sync::FileSynchronizer> syncSession;
  if (opts.hasMode || opts.hasResult) {
    syncSession = std::make_unique<sync::FileSynchronizer> (opts.resultPath);
  }

  // 1. Интерактивный выбор файла через нативный Win32 диалог OpenFileDialog, если путь не был задан
  // аргументами
  if (opts.filePath.empty ()) {
    opts.filePath = app::OpenFileDialog ();
    if (opts.filePath.empty ()) {
      std::cout << loc::tr ("main.file_not_selected") << "\n";
      return 0;
    }
  }

  // 2. Загрузка файла и построение частотного индекса слов с замером времени выполнения
  app::DocumentMenu menu (opts.filePath);
  if (!menu.isLoaded ()) {
    std::cout << loc::tr ("main.load_failed") << opts.filePath << "\n";
    return 1;
  }

  // 3. Автоматический режим (CLI): если передан параметр --mode, сразу сохраняем результат в файл и выходим
  if (opts.hasMode) {
    if (menu.saveModeReport (opts.mode, opts.resultPath, 10)) {
      std::cout << loc::tr ("main.result_saved") << opts.resultPath << "\n";
      return 0;
    } else {
      std::cerr << loc::tr ("main.result_error") << opts.resultPath << "\n";
      return 1;
    }
  }

  // 4. Интерактивный режим: запуск циклического меню в консоли с автоопределением раскладки
  menu.runInteractiveMenu ();

  return 0;
}
