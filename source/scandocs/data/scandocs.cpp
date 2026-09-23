#include "scandocs.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace core {

ScanDocs::ScanDocs (const std::filesystem::path &path) {
  buffer_words_ = readFile (path);
  if (buffer_words_.empty ())
    return;

  size_ = buffer_words_.size ();

  toLowerCaseInPlace (buffer_words_);

  words_ = extractWordsVec (buffer_words_);

  frequency_ = extractFrequency (words_);

  uniqueWords_.reserve (frequency_.size ());
  for (const auto &item : frequency_) {
    uniqueWords_.emplace_back (item.first);
  }
}

ScanDocs::~ScanDocs () = default;

// ---Геттеры---
const std::vector<std::string_view> &ScanDocs::getAllWords () const { return words_; }

const std::vector<std::string_view> &ScanDocs::getUniqueWords () const { return uniqueWords_; }

const std::vector<std::pair<std::string_view, uint32_t>> &ScanDocs::getFrequency () const {
  return frequency_;
}
//---

// ---Вспомогательные методы---
std::string ScanDocs::readFile (const std::filesystem::path &path) {
  std::ifstream file (path, std::ios::binary | std::ios::ate);

  if (!file.is_open ())
    return {};

  const auto file_size = file.tellg ();
  file.seekg (0, std::ios::beg);

  std::string buffer (file_size, '\0');
  file.read (buffer.data (), file_size);

  return buffer;
}

std::vector<std::string_view> ScanDocs::extractWordsSet (const std::vector<std::string_view> &words) {
  std::unordered_set<std::string_view> uWords (words.begin (), words.end ());
  std::vector<std::string_view> v_uWords (uWords.begin (), uWords.end ());

  return v_uWords;
}
std::vector<std::string_view> ScanDocs::extractWordsVec (std::string_view buffer) {
  std::vector<std::string_view> words;

  size_t start = 0;
  bool in_word = false;
  const size_t len = buffer.size ();
  words.reserve (len / 6);
  const auto *p = reinterpret_cast<const uint8_t *> (buffer.data ());

  for (size_t i = 0; i < len;) {
    size_t char_len = getLetterLength (p + i, len - i);
    if (char_len > 0) {
      if (!in_word) {
        start = i;
        in_word = true;
      }
      i += char_len; // перешагиваем на следующий символ (1 или 2 байта)
    } else {
      if (in_word) {
        words.emplace_back (buffer.substr (start, i - start));
        in_word = false;
      }
      ++i;
    }
  }
  if (in_word) {
    words.emplace_back (buffer.substr (start, len - start));
  }
  return words;
}

void ScanDocs::toLowerCaseInPlace (std::string &buffer) {
  auto *p = reinterpret_cast<uint8_t *> (buffer.data ());
  const size_t len = buffer.size ();

  for (size_t i = 0; i < len; ++i) {
    if (p[i] >= 'A' && p[i] <= 'Z') {
      p[i] |= 0x20; // += 32

    } else if (p[i] == 0xD0 && i + 1 < len) {
      uint8_t next = p[i + 1];

      if (next >= 0x90 && next <= 0x9F) {
        p[i + 1] += 0x20;
        ++i;
      } else if (next >= 0xA0 && next <= 0xAF) {
        p[i] = 0xD1;
        p[i + 1] -= 0x20;
        ++i;
      } else if (next == 0x81) {
        p[i] = 0xD1;
        p[i + 1] = 0x91;
        ++i;
      }
    }
  }
}

bool ScanDocs::isWordChar (uint8_t c) { return (c >= 128) || (c >= 'a' && c <= 'z'); }

std::vector<std::pair<std::string_view, uint32_t>>
ScanDocs::extractFrequency (const std::vector<std::string_view> &words) {
  if (words.empty ())
    return {};

  std::vector<std::string_view> sortedWords = words;
  std::sort (sortedWords.begin (), sortedWords.end ());

  std::vector<std::pair<std::string_view, uint32_t>> freq;
  freq.reserve (sortedWords.size () / 6);
  freq.push_back ({sortedWords[0], 1});

  for (size_t i = 1; i < sortedWords.size (); ++i) {
    if (sortedWords[i] == freq.back ().first)
      freq.back ().second++;
    else
      freq.push_back ({sortedWords[i], 1});
  }

  std::sort (freq.begin (), freq.end (), [] (const auto &a, const auto &b) {
    if (a.second != b.second)
      return a.second > b.second;
    return a.first > b.first;
  });

  return freq;
}

size_t ScanDocs::getLetterLength (const uint8_t *p, size_t remaining) {
  // английская буква (a-z) — 1 байт
  if (*p >= 'a' && *p <= 'z') {
    return 1;
  }
  // русская строчная буква в UTF-8 — строго 2 байта
  if (remaining >= 2) {
    if (*p == 0xD0 && p[1] >= 0xB0 && p[1] <= 0xBF) {
      return 2; // 'а' .. 'п'
    }
    if (*p == 0xD1 && ((p[1] >= 0x80 && p[1] <= 0x8F) || p[1] == 0x91)) {
      return 2; // 'р' .. 'я' или 'ё'
    }
  }
  return 0;
}

std::vector<std::pair<std::string_view, std::vector<uint32_t>>> ScanDocs::buildWordIndex () const {
  if (words_.empty ())
    return {};

  // Пары: (слово, индекс в тексте начиная с 0)
  std::vector<std::pair<std::string_view, uint32_t>> wordPositions;
  wordPositions.reserve (words_.size ());

  for (size_t i = 0; i < words_.size (); ++i) {
    wordPositions.emplace_back (words_[i], static_cast<uint32_t> (i));
  }

  // Сортируем по слову, а при равенстве по возрастанию позиции
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
      return a.second.size () > b.second.size (); // по убыванию количества
    return a.first < b.first;                     // при равенстве — по алфавиту
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

} // namespace core