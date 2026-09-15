#include "findlabel.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace core {

// ---FindLabel---
FindLabel::FindLabel (const std::filesystem::path &path) {

  buffer_words_ = readFile (path);
  size_ = buffer_words_.size ();

  words_ = extractWordsVec (buffer_words_);
  uniqueWords_ = extractWordsSet (words_);
}

FindLabel::~FindLabel () = default;
//---

// ---Вспомогательные методы---
std::string FindLabel::readFile (const std::filesystem::path &path) {
  std::ifstream file (path, std::ios::binary, std::ios::ate);

  if (!file.is_open ())
    return {};

  const auto file_size = file.tellg ();
  file.seekg (0, std::ios::beg);

  std::string buffer (file_size, '\0');
  file.read (buffer.data (), file_size);

  return buffer;
}

std::vector<std::string_view> FindLabel::extractWordsSet (const std::vector<std::string_view> &words) {
  std::unordered_set<std::string_view> uWords (words.begin (), words.end ());
  std::vector<std::string_view> v_uWords (uWords.begin (), uWords.end ());
  return v_uWords;
}

std::vector<std::string_view> FindLabel::extractWordsVec (const std::vector<std::string_view> &buffer) {
  std::vector<std::string_view> words;
  words.reserve (size);
  std::string_view view (buffer);

  size_t start = 0;
  size_t len = view.size ();

  for (size_t i = 0; i < len; i++) {
    if (buffer[i] == ' ' || buffer[i] == '\n' || buffer[i] == '\r') {
      if (i > start) {
        words.emplace_back (view.substr (start, i - start));
      }
      start = i + 1;
    }
  }
  if (len > start)
    words.emplace_back (view.substr (start, len - start));

  return words;
}

} // namespace core