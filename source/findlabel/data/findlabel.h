#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace core {

class FindLabel {
public:
  explicit FindLabel (const std::filesystem::path &path); // на этапе конструктора сразу читаем
                                                          // размер файла и выделяем вектор
  ~FindLabel ();
  // геттеры
  const std::vector<std::string_view> &getAllWords () const;
  const std::vector<std::string_view> &getUniqueWords () const;

private:
  size_t size_;
  std::string buffer_words_;
  std::vector<std::string_view> words_;
  std::vector<std::string_view> uniqueWords_;

  // ---Вспомогательные методы---
  std::string readFile (const std::filesystem::path &path);
  std::vector<std::string_view> extractWordsSet (const std::vector<std::string_view> &words);
  std::vector<std::string_view> extractWordsVec (const std::vector<std::string_view> &buffer);
  // метод парсинга слов в вектор
  // метод парсинга уникальных слов
  // метод
};

} // namespace core