#pragma once

#include <deque>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using WinHandle = void *;

namespace core {
/**
 * @brief Класс для сканирования документов и подсчета частоты слов.
 *
 */
class ScanDocs {
public:
  /// @brief Конструктор объекта ScanDocs
  /// @param path Путь к файлу
  explicit ScanDocs (const std::filesystem::path &path);
  /// @brief Деструктор объекта ScanDocs
  ~ScanDocs ();

  // ---Геттеры---
  /// @brief Возвращает все слова из файла
  const std::vector<std::string_view> &getAllWords () const;
  /// @brief Возвращает уникальные слова из файла
  const std::vector<std::string_view> &getUniqueWords () const;
  /// @brief Возвращает частоту встречаемости слов
  const std::vector<std::pair<std::string_view, uint32_t>> &getFrequency () const;

  /// @brief Печатает индекс слов в поток
  void printWordIndex (std::ostream &out, size_t limit = (size_t)-1) const;
  /// @brief Печатает частоту встречаемости слов в поток
  void printWordCounts (std::ostream &out, size_t limit = (size_t)-1) const;
  /// @brief Возвращает индекс слов в виде строки
  std::string getWordIndexString (size_t limit = (size_t)-1) const;
  /// @brief Возвращает частоту встречаемости слов в виде строки
  std::string getWordCountsString (size_t limit = (size_t)-1) const;

private:
  // Размер файла
  size_t size_ {0};

  // Буфер для хранения слов
  std::string buffer_words_;
  // View на буфер
  std::string_view buffer_view_;

  // Ресурсы Windows Memory-Map
  WinHandle fileHandle_ {nullptr};
  WinHandle mappingHandle_ {nullptr};
  const char *mappedData_ {nullptr};
  bool isMapped_ {false};

  // Порог 4 ГБ
  static constexpr size_t MMF_THRESHOLD = 4ULL * 1024 * 1024 * 1024;

  // Хранилище строк уникальных слов (deque не инвалидирует ссылки/указатели при push_back)
  std::deque<std::string> uniqueWordsStorage_;

  std::vector<std::string_view> words_;
  std::vector<std::string_view> uniqueWords_;
  std::vector<std::pair<std::string_view, uint32_t>> frequency_;

  // ---Вспомогательные методы---
  bool loadFile (const std::filesystem::path &path);
  void cleanupMapping ();

  // Потоковая обработка указателями
  void processWordsStreaming (std::string_view buffer);
  static bool appendLowerLetter (const uint8_t *&p, const uint8_t *end, std::string &word);

  bool isWordChar (uint8_t c);
  std::vector<std::pair<std::string_view, std::vector<uint32_t>>> buildWordIndex () const;
};

} // namespace core