#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace core {

class FindLabel {
public:
  explicit FindLabel (const std::filesystem::path &path);
  ~FindLabel ();

  // Запрет копирования во избежание висячих string_view
  FindLabel (const FindLabel &) = delete;
  FindLabel &operator= (const FindLabel &) = delete;
  FindLabel (FindLabel &&) noexcept = default;
  FindLabel &operator= (FindLabel &&) noexcept = default;

  // Геттеры
  const std::vector<std::string_view> &getUniqueWords () const;
  const std::vector<std::string_view> &getAllWords () const;

private:
  std::size_t size_;
  std::string buffer_words_;
  std::vector<std::pair<std::string_view, size_t>> wordPositions_;
  std::vector<std::string_view> words_;
  std::vector<std::string_view> uniqueWords_;

  // ---Вспомогательные методы---
  std::string readFile (const std::filesystem::path &path, std::size_t size);
  std::vector<std::pair<std::string, size_t>> extractWords (const std::string &buffer);
  std::vector<std::string_view>
  extractUniqueWords (std::vector<std::pair<std::string_view, size_t>> &wordPositions);
};

} // namespace core