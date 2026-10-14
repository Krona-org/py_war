#include "scandocs.h"

#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace core {

ScanDocs::ScanDocs (const std::filesystem::path &path) {
  if (!loadFile (path)) {
    std::cerr << "Ошибка: не удалось прочитать/отобразить файл\n";
    return;
  }

  size_ = buffer_view_.size ();

  // Запуск однопроходной обработки на арифметике указателей
  processWordsStreaming (buffer_view_);
}

ScanDocs::~ScanDocs () { cleanupMapping (); }

// ---Геттеры---
const std::vector<std::string_view> &ScanDocs::getAllWords () const { return words_; }

const std::vector<std::string_view> &ScanDocs::getUniqueWords () const { return uniqueWords_; }

const std::vector<std::pair<std::string_view, uint32_t>> &ScanDocs::getFrequency () const {
  return frequency_;
}
//---

// Проверка и приведение символа к нижнему регистру на лету (UTF-8 русский + ASCII)
bool ScanDocs::appendLowerLetter (const uint8_t *&p, const uint8_t *end, std::string &word) {
  if (p >= end)
    return false;

  uint8_t c = *p;

  // Английские буквы a-z / A-Z
  if (c >= 'a' && c <= 'z') {
    word.push_back (static_cast<char> (c));
    ++p;
    return true;
  }
  if (c >= 'A' && c <= 'Z') {
    word.push_back (static_cast<char> (c | 0x20));
    ++p;
    return true;
  }

  // Русские буквы в кодировке UTF-8 (2 байта)
  if (p + 1 < end) {
    uint8_t next = p[1];
    if (c == 0xD0) {
      if (next >= 0xB0 && next <= 0xBF) { // 'а' .. 'п' (уже строчные)
        word.push_back (static_cast<char> (c));
        word.push_back (static_cast<char> (next));
        p += 2;
        return true;
      }
      if (next >= 0x90 && next <= 0x9F) { // 'А' .. 'П' -> приводим к 'а' .. 'п'
        word.push_back (static_cast<char> (c));
        word.push_back (static_cast<char> (next + 0x20));
        p += 2;
        return true;
      }
      if (next >= 0xA0 && next <= 0xAF) { // 'Р' .. 'Я' -> приводим к 'р' .. 'я' (0xD1, next - 0x20)
        word.push_back (static_cast<char> (0xD1));
        word.push_back (static_cast<char> (next - 0x20));
        p += 2;
        return true;
      }
      if (next == 0x81) { // 'Ё' -> 'ё' (0xD1, 0x91)
        word.push_back (static_cast<char> (0xD1));
        word.push_back (static_cast<char> (0x91));
        p += 2;
        return true;
      }
    } else if (c == 0xD1) {
      if ((next >= 0x80 && next <= 0x8F) || next == 0x91) { // 'р' .. 'я' или 'ё'
        word.push_back (static_cast<char> (c));
        word.push_back (static_cast<char> (next));
        p += 2;
        return true;
      }
    }
  }

  return false;
}

// Однопроходный парсинг и подсчет частоты слов через арифметику указателей
void ScanDocs::processWordsStreaming (std::string_view buffer) {
  if (buffer.empty ())
    return;

  std::unordered_map<std::string_view, uint32_t> freqMap;
  freqMap.reserve (65536);

  const auto *p = reinterpret_cast<const uint8_t *> (buffer.data ());
  const auto *end = p + buffer.size ();

  std::string currentWord;
  currentWord.reserve (64);

  words_.clear ();
  words_.reserve (buffer.size () / 6);
  uniqueWordsStorage_.clear ();

  while (p < end) {
    // 1. Пропускаем разделители
    while (p < end) {
      const auto *prev = p;
      if (appendLowerLetter (p, end, currentWord)) {
        // Найдена первая буква слова, символ уже добавлен в currentWord
        break;
      }
      p = prev + 1;
    }

    if (p >= end && currentWord.empty ())
      break;

    // 2. Считываем остальные буквы слова
    while (p < end) {
      if (!appendLowerLetter (p, end, currentWord)) {
        break;
      }
    }

    // 3. Добавляем слово в хэш-таблицу (сложность O(1)) и words_
    if (!currentWord.empty ()) {
      auto it = freqMap.find (currentWord);
      if (it == freqMap.end ()) {
        uniqueWordsStorage_.push_back (std::move (currentWord));
        std::string_view sv = uniqueWordsStorage_.back ();
        freqMap.emplace (sv, 1);
        words_.push_back (sv);
      } else {
        it->second++;
        words_.push_back (it->first);
      }
      currentWord.clear ();
    }
  }

  // 4. Заполняем frequency_ с помощью std::string_view
  frequency_.clear ();
  frequency_.reserve (freqMap.size ());

  for (const auto &[word, count] : freqMap) {
    frequency_.emplace_back (word, count);
  }

  // 5. Сортируем: сначала по убыванию частоты, затем по алфавиту
  std::sort (frequency_.begin (), frequency_.end (), [] (const auto &a, const auto &b) {
    if (a.second != b.second)
      return a.second > b.second;
    return a.first < b.first;
  });

  // 6. Заполняем uniqueWords_ в порядке популярности
  uniqueWords_.clear ();
  uniqueWords_.reserve (frequency_.size ());
  for (const auto &item : frequency_) {
    uniqueWords_.push_back (item.first);
  }
}

bool ScanDocs::isWordChar (uint8_t c) { return (c >= 128) || (c >= 'a' && c <= 'z'); }

std::vector<std::pair<std::string_view, std::vector<uint32_t>>> ScanDocs::buildWordIndex () const {
  if (words_.empty ())
    return {};

  std::vector<std::pair<std::string_view, uint32_t>> wordPositions;
  wordPositions.reserve (words_.size ());

  for (size_t i = 0; i < words_.size (); ++i) {
    wordPositions.emplace_back (words_[i], static_cast<uint32_t> (i));
  }

  std::sort (wordPositions.begin (), wordPositions.end (), [] (const auto &a, const auto &b) {
    if (a.first != b.first)
      return a.first < b.first;
    return a.second < b.second;
  });

  std::vector<std::pair<std::string_view, std::vector<uint32_t>>> index;
  index.reserve (uniqueWords_.size ());

  for (const auto &[word, pos] : wordPositions) {
    if (index.empty () || index.back ().first != word) {
      index.emplace_back (word, std::vector<uint32_t> {pos});
    } else {
      index.back ().second.push_back (pos);
    }
  }

  std::sort (index.begin (), index.end (), [] (const auto &a, const auto &b) {
    if (a.second.size () != b.second.size ())
      return a.second.size () > b.second.size ();
    return a.first < b.first;
  });
  return index;
}

void ScanDocs::printWordIndex (std::ostream &out, size_t limit) const {
  auto index = buildWordIndex ();
  size_t count = std::min (limit, index.size ());
  for (size_t i = 0; i < count; ++i) {
    const auto &[word, positions] = index[i];
    out << "«" << word << " – ";
    for (size_t j = 0; j < positions.size (); ++j) {
      out << positions[j];
      if (j + 1 < positions.size ()) {
        out << ", ";
      }
    }
    out << "»\n";
  }
}

void ScanDocs::printWordCounts (std::ostream &out, size_t limit) const {
  size_t count = std::min (limit, frequency_.size ());
  for (size_t i = 0; i < count; ++i) {
    out << "«" << frequency_[i].first << " - " << frequency_[i].second << "»\n";
  }
}

std::string ScanDocs::getWordCountsString (size_t limit) const {
  std::ostringstream ss;
  printWordCounts (ss, limit);
  return ss.str ();
}

std::string ScanDocs::getWordIndexString (size_t limit) const {
  std::ostringstream ss;
  printWordIndex (ss, limit);
  return ss.str ();
}


void ScanDocs::cleanupMapping () {
  if (mappedData_) {
    UnmapViewOfFile (mappedData_);
    mappedData_ = nullptr;
  }
  if (mappingHandle_) {
    CloseHandle (static_cast<HANDLE> (mappingHandle_));
    mappingHandle_ = nullptr;
  }
  if (fileHandle_ && fileHandle_ != INVALID_HANDLE_VALUE) {
    CloseHandle (static_cast<HANDLE> (fileHandle_));
    fileHandle_ = nullptr;
  }
  isMapped_ = false;
}

bool ScanDocs::loadFile (const std::filesystem::path &path) {
  std::error_code ec;
  uintmax_t fileSize = std::filesystem::file_size (path, ec);

  if (ec) {
    std::cerr << "Не удалось получить размер файла: " << ec.message () << "\n";
    return false;
  }

  if (fileSize == 0) {
    buffer_view_ = {};
    return true;
  }

  // Случай 1: Файл >= 4 ГБ -> Win32 Memory-Mapped File (лучшая практика Stack Overflow)
  if (fileSize >= MMF_THRESHOLD) {
    HANDLE hFile = CreateFileW (
        path.c_str (),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, // Подсказка ОС оптимизировать предвыборку страниц
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE)
      return false;

    // В 64-битной Windows передача 0 в максимальный размер берет текущий размер файла
    HANDLE hMapping = CreateFileMappingW (hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapping) {
      CloseHandle (hFile);
      return false;
    }

    const char *pData = static_cast<const char *> (MapViewOfFile (hMapping, FILE_MAP_READ, 0, 0, 0));
    if (!pData) {
      CloseHandle (hMapping);
      CloseHandle (hFile);
      return false;
    }

    // Сохраняем дескрипторы для RAII-очистки в деструкторе
    fileHandle_ = hFile;
    mappingHandle_ = hMapping;
    mappedData_ = pData;
    isMapped_ = true;

    // buffer_view_ указывает прямо на отображенную виртуальную память
    buffer_view_ = std::string_view (mappedData_, fileSize);
    return true;
  }

  // Случай 2: Файл < 4 ГБ -> Чтение в RAM
  std::ifstream file (path, std::ios::binary);
  if (!file.is_open ())
    return false;

  buffer_words_.resize (fileSize);
  file.read (buffer_words_.data (), fileSize);
  buffer_view_ = buffer_words_;
  isMapped_ = false;
  return true;
}

} // namespace core