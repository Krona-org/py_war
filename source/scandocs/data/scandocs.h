#pragma once

#include <deque>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace core {

/**
 * @brief Класс для анализа текстовых файлов
 * @details Класс выполняет потоковую обработку файлов с использованием MMF
 */
class ScanDocs {
public:
  /// @brief Тип для хранения частоты слов
  using WordFrequency = std::vector<std::pair<std::string_view, uint32_t>>;
  /// @brief Тип для хранения индекса слов
  using WordIndex = std::vector<std::pair<std::string_view, std::vector<uint32_t>>>;

  /// @brief Конструктор, который выполняет загрузку и обработку файла
  /// @param path Путь к файлу
  explicit ScanDocs (const std::filesystem::path &path);

  /// @brief Конструктор перемещения
  ScanDocs (ScanDocs &&other) noexcept;

  /// @brief Оператор присваивания
  ScanDocs &operator= (ScanDocs &&other) noexcept;

  /// @brief Деструктор, который освобождает ресурсы
  ~ScanDocs ();

  /// @brief Запрет копирования
  ScanDocs (const ScanDocs &) = delete;
  /// @brief Запрет присваивания
  ScanDocs &operator= (const ScanDocs &) = delete;

  /// @brief Проверка успешности загрузки и отображения файла
  /// @return true, если файл успешно открыт и отображен
  bool isLoaded () const noexcept;

  /// @brief Получение всех слов документа в порядке их следования
  /// @return Константная ссылка на вектор всех слов (string_view)
  const std::vector<std::string_view> &getAllWords () const noexcept;

  /// @brief Получение уникальных слов документа, отсортированных по убыванию частоты
  /// @return Константная ссылка на вектор уникальных слов (string_view)
  const std::vector<std::string_view> &getUniqueWords () const noexcept;

  /// @brief Получение частоты встречаемости каждого слова
  /// @return Константная ссылка на список пар (слово, количество повторений)
  const WordFrequency &getFrequency () const noexcept;

  /// @brief Получение индекса позиций всех слов в документе
  /// @return Константная ссылка на список пар (слово, список позиций в тексте)
  const WordIndex &getWordIndex () const;

private:
  void *fileHandle_ {nullptr};       ///< Дескриптор открытого файла на диске
  void *mappingHandle_ {nullptr};    ///< Дескриптор объекта проекцирования файла в память
  const char *mappedData_ {nullptr}; ///< Указатель на область виртуальной памяти процесса
  size_t fileSize_ {0};              ///< Размер файла
  bool isLoaded_ {false};            ///< Флаг статуса загрузки файла

  std::deque<std::string> normalizedStorage_; ///< Хранилище нормализированных строк
  std::vector<std::string_view> words_;       ///< Вектор всех слов в порядке следования
  std::vector<std::string_view> uniqueWords_; ///< Вектор уникальных слов
  WordFrequency frequency_;                   ///< Частота встречаемости слов
  mutable WordIndex wordIndex_;               ///< Индекс позиций всех слов

  /// Метод загрузки файла
  bool loadFile (const std::filesystem::path &path);

  /// Метод очистки памяти
  void cleanupMapping ();

  /// Метод потоковой обработки слов
  void processWordsStreaming ();

  /**
   * @brief Получение информации о букве
   * @param p Указатель на начало буквы
   * @param end Указатель на конец файла
   * @param isUpper Флаг, устанавливаемый в true, если буква в верхнем регистре
   * @return Длина буквы в байтах
   */
  static size_t getLetterInfo (const uint8_t *p, const uint8_t *end, bool &isUpper);

  /**
   * @brief Приведение буквы к нижнему регистру и добавление в строку
   * @param p Указатель на начало буквы
   * @param end Указатель на конец файла
   * @param word Строка, в которую будет добавлена буква
   */
  static void appendLowerLetter (const uint8_t *&p, const uint8_t *end, std::string &word);

  /**
   * @brief Построение индекса слов
   * @return Индекс позиций всех слов
   */
  WordIndex buildWordIndex () const;
};

} // namespace core