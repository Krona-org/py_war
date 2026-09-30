#include "menu.h"

#include <Windows.h>
#include <iostream>
#include <string>

int main (int argc, char *argv[]) {
  // Установка кодировки UTF-8 для корректного отображения русского языка
  SetConsoleCP (65001);
  SetConsoleOutputCP (65001);
  std::ios::sync_with_stdio (false);

  std::string filePath;
  int mode = -1; // -1: не задан, 0: запись в файл, 1: интерактивное меню

  //  Разбор аргументов командной строки
  if (argc >= 3) {
    filePath = argv[1];
    std::string modeStr = argv[2];
    if (modeStr == "0") {
      mode = 0;
    } else if (modeStr == "1") {
      mode = 1;
    }
  } else if (argc == 2) {
    std::string arg1 = argv[1];
    if (arg1 == "0" || arg1 == "1") {
      mode = (arg1 == "0") ? 0 : 1;
    } else {
      filePath = arg1;
    }
  }

  // Если путь к файлу не был передан в аргументах, вызываем окно выбора
  if (filePath.empty ()) {
    filePath = app::OpenFileDialog ();
    if (filePath.empty ()) {
      std::cout << "Файл не выбран. Завершение работы.\n";
      return 0;
    }
  }

  // Загружаем и анализируем документ
  app::DocumentMenu menu (filePath);
  if (!menu.isLoaded ()) {
    std::cout << "Ошибка: не удалось загрузить или отобразить файл: " << filePath << "\n";
    return 1;
  }

  // Если режим не был указан в аргументах (0 или 1), запрашиваем у пользователя
  if (mode == -1) {
    menu.printHeader ();
    std::cout << "Выберите режим работы программы:\n";
    std::cout << "  1 - Показать интерактивное циклическое меню\n";
    std::cout << "  0 - Записать отчет в файл и выйти\n";
    std::cout << "Ваш выбор [0 или 1]: ";

    std::string input;
    if (std::getline (std::cin, input)) {
      if (input == "0") {
        mode = 0;
      } else {
        mode = 1; // по умолчанию при любом другом вводе
      }
    } else {
      mode = 1;
    }
  }

  // Выполнение выбранного режима
  if (mode == 0) {
    // Режим 0: запись отчета в файл (без потокового вывода на экран)
    const std::string outputFile = "report.txt";
    if (menu.saveReportToFile (outputFile, 10, 10)) {
      std::cout << "Отчет успешно записан в файл: " << outputFile << "\n";
      return 0;
    } else {
      std::cerr << "Ошибка при записи отчета в файл: " << outputFile << "\n";
      return 1;
    }
  } else {
    // Запуск интерактивного циклического меню
    menu.runInteractiveMenu ();
  }

  return 0;
}
