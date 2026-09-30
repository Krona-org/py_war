#include "menu.h"

#include <Windows.h>
#include <commdlg.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace app {

std::string OpenFileDialog () {
  OPENFILENAMEA ofn;
  char szFile[MAX_PATH] = {0};

  ZeroMemory (&ofn, sizeof (ofn));
  ofn.lStructSize = sizeof (ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof (szFile);
  ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

  if (GetOpenFileNameA (&ofn)) {
    return std::string (szFile);
  }
  return "";
}

DocumentMenu::DocumentMenu (const std::string &filePath) {
  if (!filePath.empty ()) {
    loadFile (filePath);
  }
}

bool DocumentMenu::loadFile (const std::string &filePath) {
  filePath_ = filePath;
  processingTimeMs_ = 0.0;

  auto startTime = std::chrono::high_resolution_clock::now ();

  scanDocs_ = core::ScanDocs (filePath_);

  if (!scanDocs_.isLoaded ()) {
    return false;
  }

  // Предварительно строим индекс слов, чтобы замерить полное время обработки документа
  scanDocs_.getWordIndex ();

  auto endTime = std::chrono::high_resolution_clock::now ();
  processingTimeMs_ = std::chrono::duration<double, std::milli> (endTime - startTime).count ();

  return true;
}

bool DocumentMenu::isLoaded () const noexcept { return scanDocs_.isLoaded (); }

void DocumentMenu::printHeader () const {
  std::cout << "\n==================================================\n";
  std::cout << "Файл: " << filePath_ << "\n";
  if (isLoaded ()) {
    double sizeKb = static_cast<double> (scanDocs_.getFileSize ()) / 1024.0;
    double sizeMb = sizeKb / 1024.0;
    std::cout << "Размер: " << scanDocs_.getFileSize () << " байт";
    if (sizeMb >= 1.0) {
      std::cout << " (" << std::fixed << std::setprecision (2) << sizeMb << " МБ)";
    } else {
      std::cout << " (" << std::fixed << std::setprecision (2) << sizeKb << " КБ)";
    }
    std::cout << "\n";
    std::cout << "Время обработки: " << std::fixed << std::setprecision (2) << processingTimeMs_ << " мс\n";
    std::cout << "Всего слов в тексте: " << scanDocs_.getAllWords ().size () << "\n";
    std::cout << "Уникальных слов: " << scanDocs_.getUniqueWords ().size () << "\n";
  } else {
    std::cout << "[Файл не загружен или пуст]\n";
  }
  std::cout << "==================================================\n";
}

void DocumentMenu::showUniqueWords (size_t limit) const {
  if (!isLoaded ()) {
    std::cout << "Файл не загружен!\n";
    return;
  }

  const auto &freq = scanDocs_.getFrequency ();
  size_t totalUnique = scanDocs_.getUniqueWords ().size ();
  size_t count = std::min (limit, freq.size ());

  std::cout << "\n>>> Пункт 1: Уникальные слова <<<\n";
  std::cout << "Общее количество уникальных слов: " << totalUnique << "\n";
  std::cout << "Топ-" << count << " наиболее часто встречающихся слов:\n";
  std::cout << "--------------------------------------------------\n";

  for (size_t i = 0; i < count; ++i) {
    std::cout << (i + 1) << ". " << freq[i].first << " : " << freq[i].second << " раз\n";
  }
  std::cout << "--------------------------------------------------\n";
}

void DocumentMenu::showWordPositions (size_t limit, size_t maxPositionsPerWord) const {
  if (!isLoaded ()) {
    std::cout << "Файл не загружен!\n";
    return;
  }

  const auto &wordIndex = scanDocs_.getWordIndex ();
  size_t count = std::min (limit, wordIndex.size ());

  std::cout << "\n>>> Пункт 2: Индексация позиций слов <<<\n";
  std::cout << "Топ-" << count << " слов по частоте с их индексами позиций в тексте:\n";
  std::cout << "--------------------------------------------------\n";

  for (size_t i = 0; i < count; ++i) {
    const auto &[word, positions] = wordIndex[i];
    std::cout << (i + 1) << ". \"" << word << "\" (вхождений: " << positions.size () << "):\n   Позиции: [";

    size_t showCount = std::min (positions.size (), maxPositionsPerWord);
    for (size_t p = 0; p < showCount; ++p) {
      std::cout << positions[p];
      if (p + 1 < showCount) {
        std::cout << ", ";
      }
    }
    if (positions.size () > showCount) {
      std::cout << ", ... ещё " << (positions.size () - showCount) << " поз.";
    }
    std::cout << "]\n\n";
  }
  std::cout << "--------------------------------------------------\n";
}

bool DocumentMenu::saveReportToFile (const std::string &outputPath, size_t topWordsLimit,
                                     size_t topPositionsLimit) {
  if (!isLoaded ()) {
    return false;
  }

  std::ofstream out (outputPath);
  if (!out.is_open ()) {
    return false;
  }

  double sizeKb = static_cast<double> (scanDocs_.getFileSize ()) / 1024.0;
  double sizeMb = sizeKb / 1024.0;

  out << "Файл: " << filePath_ << "\n";
  out << "Размер: " << scanDocs_.getFileSize () << " байт";
  if (sizeMb >= 1.0) {
    out << " (" << std::fixed << std::setprecision (2) << sizeMb << " МБ)";
  } else {
    out << " (" << std::fixed << std::setprecision (2) << sizeKb << " КБ)";
  }
  out << "\n";
  out << "Время обработки: " << std::fixed << std::setprecision (2) << processingTimeMs_ << " мс\n";
  out << "Всего слов в тексте: " << scanDocs_.getAllWords ().size () << "\n";
  out << "Всего уникальных слов: " << scanDocs_.getUniqueWords ().size () << "\n\n";

  // 1. Уникальные слова
  const auto &freq = scanDocs_.getFrequency ();
  size_t wordsCount = std::min (topWordsLimit, freq.size ());
  out << "1. Уникальные слова:\n";
  out << "Количество уникальных слов: " << scanDocs_.getUniqueWords ().size () << "\n";
  out << "Топ-" << wordsCount << " самых частых уникальных слов:\n";
  for (size_t i = 0; i < wordsCount; ++i) {
    out << (i + 1) << ". " << freq[i].first << " : " << freq[i].second << " вхождений\n";
  }
  out << "\n";

  // 2. Индексация позиций слов
  const auto &wordIndex = scanDocs_.getWordIndex ();
  size_t posWordsCount = std::min (topPositionsLimit, wordIndex.size ());
  out << "2. Топ индексации позиций слов (топ-" << posWordsCount << "):\n";
  for (size_t i = 0; i < posWordsCount; ++i) {
    const auto &[word, positions] = wordIndex[i];
    out << (i + 1) << ". \"" << word << "\" (всего " << positions.size () << " позиций):\n   [";
    size_t showPos = std::min<size_t> (positions.size (), 20);
    for (size_t p = 0; p < showPos; ++p) {
      out << positions[p];
      if (p + 1 < showPos) {
        out << ", ";
      }
    }
    if (positions.size () > showPos) {
      out << ", ... ещё " << (positions.size () - showPos) << " поз.";
    }
    out << "]\n";
  }
  out << "==================================================\n";

  return true;
}

void DocumentMenu::runInteractiveMenu () {
  while (true) {
    printHeader ();
    std::cout << "МЕНЮ ДЕЙСТВИЙ:\n";
    std::cout << "  1. Показать уникальные слова (по умолчанию топ-10)\n";
    std::cout << "  2. Показать топ индексации позиций слов (по умолчанию топ-10)\n";
    std::cout << "  3. Записать отчет в файл\n";
    std::cout << "  4. Открыть другой файл\n";
    std::cout << "  0. Выход\n";
    std::cout << "Выберите пункт [0-4]: ";

    std::string choice;
    if (!std::getline (std::cin, choice)) {
      break;
    }

    if (choice.empty ()) {
      continue;
    }

    if (choice == "0") {
      std::cout << "Завершение работы программы.\n";
      break;
    } else if (choice == "1") {
      std::cout << "Введите количество слов для топа (Enter для 10): ";
      std::string limitStr;
      std::getline (std::cin, limitStr);
      size_t limit = 10;
      if (!limitStr.empty ()) {
        try {
          limit = std::stoul (limitStr);
        } catch (...) {
          limit = 10;
        }
      }
      showUniqueWords (limit);
      std::cout << "\nНажмите Enter для возврата в меню...";
      std::string dummy;
      std::getline (std::cin, dummy);
    } else if (choice == "2") {
      std::cout << "Введите количество слов для вывода позиций (Enter для 10): ";
      std::string limitStr;
      std::getline (std::cin, limitStr);
      size_t limit = 10;
      if (!limitStr.empty ()) {
        try {
          limit = std::stoul (limitStr);
        } catch (...) {
          limit = 10;
        }
      }
      showWordPositions (limit);
      std::cout << "\nНажмите Enter для возврата в меню...";
      std::string dummy;
      std::getline (std::cin, dummy);
    } else if (choice == "3") {
      std::cout << "Введите имя файла для отчета (Enter для 'report.txt'): ";
      std::string outPath;
      std::getline (std::cin, outPath);
      if (outPath.empty ()) {
        outPath = "report.txt";
      }
      if (saveReportToFile (outPath)) {
        std::cout << "Отчет успешно сохранен в файл: " << outPath << "\n";
      } else {
        std::cout << "Ошибка при записи отчета в файл!\n";
      }
      std::cout << "\nНажмите Enter для возврата в меню...";
      std::string dummy;
      std::getline (std::cin, dummy);
    } else if (choice == "4") {
      std::cout << "Открыть через диалоговое окно (1) или ввести путь вручную (2)? [1]: ";
      std::string openType;
      std::getline (std::cin, openType);
      std::string newPath;
      if (openType == "2") {
        std::cout << "Введите полный путь к файлу: ";
        std::getline (std::cin, newPath);
      } else {
        newPath = OpenFileDialog ();
      }

      if (newPath.empty ()) {
        std::cout << "Файл не выбран.\n";
      } else {
        std::cout << "Загрузка файла " << newPath << " ...\n";
        if (loadFile (newPath)) {
          std::cout << "Файл успешно загружен!\n";
        } else {
          std::cout << "Ошибка: не удалось загрузить файл.\n";
        }
      }
      std::cout << "\nНажмите Enter для возврата в меню...";
      std::string dummy;
      std::getline (std::cin, dummy);
    } else {
      std::cout << "Неизвестный пункт меню. Повторите ввод.\n";
    }
  }
}

} // namespace app
