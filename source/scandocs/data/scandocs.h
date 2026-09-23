#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace core {

class ScanDocs {
public:
  explicit ScanDocs (const std::filesystem::path &path); // на этапе конструктора сразу читаем
                                                         // размер файла и выделяем вектор
  ~ScanDocs ();
  // ---Геттеры---
  const std::vector<std::string_view> &getAllWords () const;
  const std::vector<std::string_view> &getUniqueWords () const;
  const std::vector<std::pair<std::string_view, uint32_t>> &getFrequency () const;

  void printWordIndex (std::ostream &out, size_t limit) const;

private:
  size_t size_;
  std::string buffer_words_;
  std::vector<std::string_view> words_;
  std::vector<std::string_view> uniqueWords_;
  std::vector<std::pair<std::string_view, uint32_t>> frequency_;

  // ---Вспомогательные методы---
  bool isWordChar (uint8_t c);
  void toLowerCaseInPlace (std::string &buffer);
  size_t getLetterLength (const uint8_t *p, size_t remaining);
  std::string readFile (const std::filesystem::path &path);
  std::vector<std::string_view> extractWordsSet (const std::vector<std::string_view> &words);
  std::vector<std::string_view> extractWordsVec (std::string_view buffer);
  std::vector<std::pair<std::string_view, uint32_t>>
  extractFrequency (const std::vector<std::string_view> &words);
  std::vector<std::pair<std::string_view, std::vector<uint32_t>>> buildWordIndex () const;
};

} // namespace core