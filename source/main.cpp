#include "scandocs.h"

#include <Windows.h>
#include <algorithm>
#include <iostream>
#include <string>

/// @brief Функция вызова диалогового окна выбора файла
/// @return Путь к выбранному файлу
std::string OpenFileDialog () {
  OPENFILENAMEA ofn;           /// Структура для хранения параметров диалогового окна
  char szFile[MAX_PATH] = {0}; /// Массив для хранения пути к файлу

  ZeroMemory (&ofn, sizeof (ofn));                                       /// Обнуление структуры
  ofn.lStructSize = sizeof (ofn);                                        /// Размер структуры
  ofn.hwndOwner = NULL;                                                  /// Владелец окна
  ofn.lpstrFile = szFile;                                                /// Пусть к файлу
  ofn.nMaxFile = sizeof (szFile);                                        /// Максимальный размер пути
  ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0"; /// Фильтр показываемых файлов
  ofn.nFilterIndex = 1;                                                  /// Индекс фильтра (1 = .txt)
  ofn.Flags = OFN_PATHMUSTEXIST                                          /// Путь должен существовать
              | OFN_FILEMUSTEXIST                                        /// Файл должен существовать
              | OFN_NOCHANGEDIR;                                         /// Не менять текущую директорию

  if (GetOpenFileNameA (&ofn)) {
    return std::string (szFile);
  }
  return "";
}

/// @brief Главная функция
/// @param argc Количество аргументов командной строки
/// @param argv Аргументы командной строки
int main (int argc, char *argv[]) {
  SetConsoleCP (65001);
  SetConsoleOutputCP (65001);
  std::ios::sync_with_stdio (false);

  std::string filePath (OpenFileDialog ());

  core::ScanDocs scanDocs (filePath);
  return 0;
}
