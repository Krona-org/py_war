#include "menu.h"
#include "file_sync.h"
#include "localization.h"

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
  std::cout << "\n" << loc::tr ("header.divider") << "\n";
  std::cout << loc::tr ("header.file") << filePath_ << "\n";
  if (isLoaded ()) {
    double sizeKb = static_cast<double> (scanDocs_.getFileSize ()) / 1024.0;
    double sizeMb = sizeKb / 1024.0;
    std::cout << loc::tr ("header.size") << scanDocs_.getFileSize () << " " << loc::tr ("header.bytes");
    if (sizeMb >= 1.0) {
      std::cout << " (" << std::fixed << std::setprecision (2) << sizeMb << " " << loc::tr ("header.mb") << ")";
    } else {
      std::cout << " (" << std::fixed << std::setprecision (2) << sizeKb << " " << loc::tr ("header.kb") << ")";
    }
    std::cout << "\n";
    std::cout << loc::tr ("header.time") << std::fixed << std::setprecision (2) << processingTimeMs_ << " "
              << loc::tr ("header.ms") << "\n";
    std::cout << loc::tr ("header.total_words") << scanDocs_.getAllWords ().size () << "\n";
    std::cout << loc::tr ("header.unique_words") << scanDocs_.getUniqueWords ().size () << "\n";
  } else {
    std::cout << loc::tr ("header.file_empty") << "\n";
  }
  std::cout << loc::tr ("header.divider") << "\n";
}

void DocumentMenu::showUniqueWords (size_t limit) const {
  if (!isLoaded ()) {
    std::cout << loc::tr ("header.file_not_loaded") << "\n";
    return;
  }

  const auto &freq = scanDocs_.getFrequency ();
  size_t totalUnique = scanDocs_.getUniqueWords ().size ();
  size_t count = std::min (limit, freq.size ());

  std::cout << "\n" << loc::tr ("report.section1_console") << "\n";
  std::cout << loc::tr ("report.unique_count") << totalUnique << "\n";
  std::cout << loc::tr ("report.top_words_console") << count << loc::tr ("report.top_words_console_suffix") << "\n";
  std::cout << loc::tr ("header.subdivider") << "\n";

  for (size_t i = 0; i < count; ++i) {
    std::cout << (i + 1) << ". " << freq[i].first << " : " << freq[i].second << " " << loc::tr ("report.times") << "\n";
  }
  std::cout << loc::tr ("header.subdivider") << "\n";
}

void DocumentMenu::showWordPositions (size_t limit, size_t maxPositionsPerWord) const {
  if (!isLoaded ()) {
    std::cout << loc::tr ("header.file_not_loaded") << "\n";
    return;
  }

  const auto &wordIndex = scanDocs_.getWordIndex ();
  size_t count = std::min (limit, wordIndex.size ());

  std::cout << "\n" << loc::tr ("report.section2_console") << "\n";
  std::cout << loc::tr ("report.top_words_console") << count << loc::tr ("report.top_pos_console_suffix") << "\n";
  std::cout << loc::tr ("header.subdivider") << "\n";

  for (size_t i = 0; i < count; ++i) {
    const auto &[word, positions] = wordIndex[i];
    std::cout << (i + 1) << ". \"" << word << "\" (" << loc::tr ("report.occurrences") << ": " << positions.size ()
              << "):\n   Позиции: [";

    size_t showCount = std::min (positions.size (), maxPositionsPerWord);
    for (size_t p = 0; p < showCount; ++p) {
      std::cout << positions[p];
      if (p + 1 < showCount) {
        std::cout << ", ";
      }
    }
    if (positions.size () > showCount) {
      std::cout << loc::tr ("report.more_positions") << (positions.size () - showCount)
                << loc::tr ("report.positions_suffix");
    }
    std::cout << "]\n\n";
  }
  std::cout << loc::tr ("header.subdivider") << "\n";
}

bool DocumentMenu::saveReportToFile (const std::string &outputPath, size_t topWordsLimit,
                                     size_t topPositionsLimit) {
  if (!isLoaded ()) {
    return false;
  }

  std::ostringstream out;

  double sizeKb = static_cast<double> (scanDocs_.getFileSize ()) / 1024.0;
  double sizeMb = sizeKb / 1024.0;

  out << loc::tr ("header.file") << filePath_ << "\n";
  out << loc::tr ("header.size") << scanDocs_.getFileSize () << " " << loc::tr ("header.bytes");
  if (sizeMb >= 1.0) {
    out << " (" << std::fixed << std::setprecision (2) << sizeMb << " " << loc::tr ("header.mb") << ")";
  } else {
    out << " (" << std::fixed << std::setprecision (2) << sizeKb << " " << loc::tr ("header.kb") << ")";
  }
  out << "\n";
  out << loc::tr ("header.time") << std::fixed << std::setprecision (2) << processingTimeMs_ << " "
      << loc::tr ("header.ms") << "\n";
  out << loc::tr ("header.total_words") << scanDocs_.getAllWords ().size () << "\n";
  out << loc::tr ("header.unique_words") << scanDocs_.getUniqueWords ().size () << "\n\n";

  // 1. Уникальные слова
  const auto &freq = scanDocs_.getFrequency ();
  size_t wordsCount = std::min (topWordsLimit, freq.size ());
  out << loc::tr ("report.unique_title") << wordsCount << loc::tr ("report.unique_by_freq") << "\n";
  out << loc::tr ("report.unique_count") << scanDocs_.getUniqueWords ().size () << "\n";
  for (size_t i = 0; i < wordsCount; ++i) {
    out << (i + 1) << ". " << freq[i].first << " : " << freq[i].second << " " << loc::tr ("report.occurrences") << "\n";
  }
  out << "\n";

  // 2. Индексация позиций слов
  const auto &wordIndex = scanDocs_.getWordIndex ();
  size_t posWordsCount = std::min (topPositionsLimit, wordIndex.size ());
  out << loc::tr ("report.positions_title") << posWordsCount << "):\n";
  for (size_t i = 0; i < posWordsCount; ++i) {
    const auto &[word, positions] = wordIndex[i];
    out << (i + 1) << ". \"" << word << "\" (" << loc::tr ("report.positions_prefix") << positions.size () << "):\n   [";
    size_t showPos = std::min<size_t> (positions.size (), 20);
    for (size_t p = 0; p < showPos; ++p) {
      out << positions[p];
      if (p + 1 < showPos) {
        out << ", ";
      }
    }
    if (positions.size () > showPos) {
      out << loc::tr ("report.more_positions") << (positions.size () - showPos)
          << loc::tr ("report.positions_suffix");
    }
    out << "]\n";
  }
  out << loc::tr ("header.divider");

  return sync::FileSynchronizer::writeSynchronized (outputPath, out.str ());
}

bool DocumentMenu::saveModeReport (int mode, const std::string &outputPath, size_t limit) {
  if (mode == 0) {
    return saveReportToFile (outputPath, limit, limit);
  }

  if (!isLoaded ()) {
    return false;
  }

  std::ostringstream out;

  double sizeKb = static_cast<double> (scanDocs_.getFileSize ()) / 1024.0;
  double sizeMb = sizeKb / 1024.0;

  out << loc::tr ("header.file") << filePath_ << "\n";
  out << loc::tr ("header.size") << scanDocs_.getFileSize () << " " << loc::tr ("header.bytes");
  if (sizeMb >= 1.0) {
    out << " (" << std::fixed << std::setprecision (2) << sizeMb << " " << loc::tr ("header.mb") << ")";
  } else {
    out << " (" << std::fixed << std::setprecision (2) << sizeKb << " " << loc::tr ("header.kb") << ")";
  }
  out << "\n";
  out << loc::tr ("header.time") << std::fixed << std::setprecision (2) << processingTimeMs_ << " "
      << loc::tr ("header.ms") << "\n";
  out << loc::tr ("header.total_words") << scanDocs_.getAllWords ().size () << "\n";
  out << loc::tr ("header.unique_words") << scanDocs_.getUniqueWords ().size () << "\n\n";

  if (mode == 1) {
    // 1. Уникальные слова и частота
    const auto &freq = scanDocs_.getFrequency ();
    size_t wordsCount = std::min (limit, freq.size ());
    out << loc::tr ("report.unique_title") << wordsCount << loc::tr ("report.unique_by_freq") << "\n";
    out << loc::tr ("report.unique_count") << scanDocs_.getUniqueWords ().size () << "\n";
    for (size_t i = 0; i < wordsCount; ++i) {
      out << (i + 1) << ". " << freq[i].first << " : " << freq[i].second << " " << loc::tr ("report.occurrences") << "\n";
    }
  } else if (mode == 2) {
    // 2. Индексация позиций и количество встреч слов
    const auto &wordIndex = scanDocs_.getWordIndex ();
    size_t posWordsCount = std::min (limit, wordIndex.size ());
    out << loc::tr ("report.positions_title") << posWordsCount << "):\n";
    for (size_t i = 0; i < posWordsCount; ++i) {
      const auto &[word, positions] = wordIndex[i];
      out << (i + 1) << ". \"" << word << "\" (" << loc::tr ("report.positions_prefix") << positions.size () << "):\n   [";
      size_t showPos = std::min<size_t> (positions.size (), 20);
      for (size_t p = 0; p < showPos; ++p) {
        out << positions[p];
        if (p + 1 < showPos) {
          out << ", ";
        }
      }
      if (positions.size () > showPos) {
        out << loc::tr ("report.more_positions") << (positions.size () - showPos)
            << loc::tr ("report.positions_suffix");
      }
      out << "]\n";
    }
  }
  out << loc::tr ("header.divider");

  return sync::FileSynchronizer::writeSynchronized (outputPath, out.str ());
}

void DocumentMenu::runInteractiveMenu () {
  while (true) {
    if (!loc::isExplicitLanguage ()) {
      std::string kbdLang = loc::detectKeyboardLanguage ();
      if (kbdLang != loc::getLanguage ()) {
        loc::setLanguage (kbdLang);
      }
    }
    printHeader ();
    std::cout << loc::tr ("menu.title") << "\n";
    std::cout << loc::tr ("menu.item1") << "\n";
    std::cout << loc::tr ("menu.item2") << "\n";
    std::cout << loc::tr ("menu.item3") << "\n";
    std::cout << loc::tr ("menu.item4") << "\n";
    std::cout << loc::tr ("menu.item0") << "\n";
    std::cout << loc::tr ("menu.prompt");

    std::string choice;
    if (!std::getline (std::cin, choice)) {
      break;
    }

    if (choice.empty ()) {
      continue;
    }

    if (choice == "0") {
      std::cout << loc::tr ("menu.exit_message") << "\n";
      break;
    } else if (choice == "1") {
      std::cout << loc::tr ("menu.input_words_limit");
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
      std::cout << "\n" << loc::tr ("menu.press_enter");
      std::string dummy;
      std::getline (std::cin, dummy);
    } else if (choice == "2") {
      std::cout << loc::tr ("menu.input_positions_limit");
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
      std::cout << "\n" << loc::tr ("menu.press_enter");
      std::string dummy;
      std::getline (std::cin, dummy);
    } else if (choice == "3") {
      std::cout << loc::tr ("menu.input_report_filename");
      std::string outPath;
      std::getline (std::cin, outPath);
      if (outPath.empty ()) {
        outPath = "report.txt";
      }
      if (saveReportToFile (outPath)) {
        std::cout << loc::tr ("menu.report_save_success") << outPath << "\n";
      } else {
        std::cout << loc::tr ("menu.report_save_error") << "\n";
      }
      std::cout << "\n" << loc::tr ("menu.press_enter");
      std::string dummy;
      std::getline (std::cin, dummy);
    } else if (choice == "4") {
      std::cout << loc::tr ("menu.open_dialog_choice");
      std::string openType;
      std::getline (std::cin, openType);
      std::string newPath;
      if (openType == "2") {
        std::cout << loc::tr ("menu.input_file_path");
        std::getline (std::cin, newPath);
      } else {
        newPath = OpenFileDialog ();
      }

      if (newPath.empty ()) {
        std::cout << loc::tr ("menu.file_not_selected") << "\n";
      } else {
        std::cout << loc::tr ("menu.loading_file") << newPath << " ...\n";
        if (loadFile (newPath)) {
          std::cout << loc::tr ("menu.load_success") << "\n";
        } else {
          std::cout << loc::tr ("menu.load_error") << "\n";
        }
      }
      std::cout << "\n" << loc::tr ("menu.press_enter");
      std::string dummy;
      std::getline (std::cin, dummy);
    } else {
      std::cout << loc::tr ("menu.unknown_choice") << "\n";
    }
  }
}

} // namespace app
