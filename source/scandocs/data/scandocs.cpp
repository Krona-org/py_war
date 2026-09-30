#include "scandocs.h"

#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace core {

ScanDocs::ScanDocs (const std::filesystem::path &path) {
  if (loadFile (path)) {
    processWordsStreaming ();
  }
}

ScanDocs::~ScanDocs () { cleanupMapping (); }

ScanDocs::ScanDocs (ScanDocs &&other) noexcept
    : fileHandle_ (other.fileHandle_)
    , mappingHandle_ (other.mappingHandle_)
    , mappedData_ (other.mappedData_)
    , fileSize_ (other.fileSize_)
    , isLoaded_ (other.isLoaded_)
    , normalizedStorage_ (std::move (other.normalizedStorage_))
    , words_ (std::move (other.words_))
    , uniqueWords_ (std::move (other.uniqueWords_))
    , frequency_ (std::move (other.frequency_))
    , wordIndex_ (std::move (other.wordIndex_)) {
  other.fileHandle_ = nullptr;
  other.mappingHandle_ = nullptr;
  other.mappedData_ = nullptr;
  other.fileSize_ = 0;
  other.isLoaded_ = false;
}

ScanDocs &ScanDocs::operator= (ScanDocs &&other) noexcept {
  if (this != &other) {
    cleanupMapping ();

    fileHandle_ = other.fileHandle_;
    mappingHandle_ = other.mappingHandle_;
    mappedData_ = other.mappedData_;
    fileSize_ = other.fileSize_;
    isLoaded_ = other.isLoaded_;

    normalizedStorage_ = std::move (other.normalizedStorage_);
    words_ = std::move (other.words_);
    uniqueWords_ = std::move (other.uniqueWords_);
    frequency_ = std::move (other.frequency_);
    wordIndex_ = std::move (other.wordIndex_);

    other.fileHandle_ = nullptr;
    other.mappingHandle_ = nullptr;
    other.mappedData_ = nullptr;
    other.fileSize_ = 0;
    other.isLoaded_ = false;
  }
  return *this;
}

bool ScanDocs::isLoaded () const noexcept { return isLoaded_; }

size_t ScanDocs::getFileSize () const noexcept { return fileSize_; }

// ---Геттеры данных---
const std::vector<std::string_view> &ScanDocs::getAllWords () const noexcept { return words_; }

const std::vector<std::string_view> &ScanDocs::getUniqueWords () const noexcept { return uniqueWords_; }

const ScanDocs::WordFrequency &ScanDocs::getFrequency () const noexcept { return frequency_; }

const ScanDocs::WordIndex &ScanDocs::getWordIndex () const {
  if (wordIndex_.empty () && !words_.empty ()) {
    wordIndex_ = buildWordIndex ();
  }
  return wordIndex_;
}

// Быстрое определение длины буквы и регистра (ASCII + UTF-8 русский)
size_t ScanDocs::getLetterInfo (const uint8_t *p, const uint8_t *end, bool &isUpper) {
  if (p >= end) {
    return 0;
  }

  uint8_t c = *p; ///< Текущий байт

  // Английские буквы a-z / A-Z
  if (c >= 'a' && c <= 'z') {
    isUpper = false;
    return 1;
  }
  if (c >= 'A' && c <= 'Z') {
    isUpper = true;
    return 1;
  }

  /**
   * @brief Проверка на русскую букву в UTF-8 (2 байта)
   * @param c - это проверка первого байта, к какому языку относится буква
   * @param next - это проверка второго байта на то, какая это буква
   * @return размер буквы
   */
  if (p + 1 < end) {
    uint8_t next = p[1]; /// Следующий байт

    if (c == 0xD0) {
      if ((next >= 0x90 && next <= 0xAF) || next == 0x81) { // 'А' .. 'Я', 'Ё'
        isUpper = true;
        return 2;
      }
      if (next >= 0xB0 && next <= 0xBF) { // 'а' .. 'п'
        isUpper = false;
        return 2;
      }
    } else if (c == 0xD1) {
      if ((next >= 0x80 && next <= 0x8F) || next == 0x91) { // 'р' .. 'я', 'ё'
        isUpper = false;
        return 2;
      }
    }
  }

  return 0;
}

// Приведение символа к нижнему регистру и добавление в строку
void ScanDocs::appendLowerLetter (const uint8_t *&p, const uint8_t *end, std::string &word) {
  if (p >= end) {
    return;
  }

  uint8_t c = *p; /// Текущий байт

  if (c >= 'a' && c <= 'z') {
    word.push_back (static_cast<char> (c)); /// Если буква в нижнем регистре то сразу добавляем ее в строку
    ++p;                                    /// Переходим к следующей букве
    return;
  }
  if (c >= 'A' && c <= 'Z') {
    word.push_back (static_cast<char> (
        c | 0x20)); /// Если буква в верхнем регистре то приводим ее к нижниму регистру, через побитовое или
    ++p;            /// Переходим к следующей букве
    return;
  }

  if (p + 1 < end) {
    uint8_t next = p[1];
    if (c == 0xD0) {
      if (next >= 0xB0 && next <= 0xBF) { // 'а' .. 'п'
        word.push_back (static_cast<char> (c));
        word.push_back (static_cast<char> (next));
        p += 2;
        return;
      }
      if (next >= 0x90 && next <= 0x9F) { // 'А' .. 'П' -> 'а' .. 'п'
        word.push_back (static_cast<char> (c));
        word.push_back (static_cast<char> (next + 0x20));
        p += 2;
        return;
      }
      if (next >= 0xA0 && next <= 0xAF) { // 'Р' .. 'Я' -> 'р' .. 'я'
        word.push_back (static_cast<char> (0xD1));
        word.push_back (static_cast<char> (next - 0x20));
        p += 2;
        return;
      }
      if (next == 0x81) { // 'Ё' -> 'ё'
        word.push_back (static_cast<char> (0xD1));
        word.push_back (static_cast<char> (0x91));
        p += 2;
        return;
      }
    } else if (c == 0xD1) {
      if ((next >= 0x80 && next <= 0x8F) || next == 0x91) { // 'р' .. 'я', 'ё'
        word.push_back (static_cast<char> (c));
        word.push_back (static_cast<char> (next));
        p += 2;
        return;
      }
    }
  }

  // Защита от бесконечного цикла: если байт не распознан, продвигаем указатель
  word.push_back (static_cast<char> (c));
  ++p;
}

// Потоковый разбор слова по виртуальной памяти
void ScanDocs::processWordsStreaming () {
  if (!mappedData_ || fileSize_ == 0) {
    return;
  }

  std::unordered_map<std::string_view, uint32_t> freqMap; ///< Хэш-таблица для подсчета частоты слов
  freqMap.reserve (65536);

  words_.clear ();
  words_.reserve (fileSize_ / 6);
  normalizedStorage_.clear ();
  wordIndex_.clear ();

  /// Смотрим на память файла так, будто это массив байтов
  const auto *p =
      reinterpret_cast<const uint8_t *> (mappedData_); ///< указатель на первый байт отображаемой памяти
  const auto *end = p + fileSize_;                     ///< указатель на конец отображаемой памяти

  std::string tempWord;  ///< Временная строка для хранения слова
  tempWord.reserve (64); ///< Резервируем память для слова

  /// Цикл чтения проходит по всем байтам файла, пока не достигнет конца
  while (p < end) {
    bool isUpper = false; ///< Флаг регистра
    size_t len = 0;       ///< Размер буквы в байтах

    /// Поиск начала слова
    while (p < end && (len = getLetterInfo (p, end, isUpper)) == 0)
      ++p; ///< Переходим к следующему байту

    /// Если дошли до конца файла то выходим из цикла
    if (p >= end)
      break;

    const uint8_t *wordStart = p; /// Запоминаем указатель на начало слова
    bool hasUpper = isUpper;      /// Флаг регистра для всего слова
    p += len;                     /// Сдвигаем указатель на следующую букву

    /// Итерируемся до конца слова, запоминая регистр
    while (p < end && (len = getLetterInfo (p, end, isUpper)) > 0) {
      hasUpper |= isUpper;
      p += len;
    }
    const uint8_t *wordEnd = p; ///< Указатель на конец слова

    std::string_view wordView; ///< Указатель на слово в памяти
    /// Если в слове нет букв в верхнем регистре то сразу записываем слово в string_view
    if (!hasUpper) {
      /// Первый аргуемнт указатель на начало слова, второй сколько байт он занимает
      wordView = std::string_view (reinterpret_cast<const char *> (wordStart), wordEnd - wordStart);
    } else {
      tempWord.clear ();              ///< Очищаем временное слово
      const uint8_t *cur = wordStart; ///< Указатель на начало слова

      /// Итерируемся до конца слова, приводя буквы к нижнему регистру
      while (cur < wordEnd) {
        appendLowerLetter (cur, wordEnd, tempWord); ///< Метод приведения к нижнему регистру
      }

      wordView = tempWord; ///< Привязываем string_view к буферу временной строки
    }

    /// Поиск слова в хэш таблице
    auto it = freqMap.find (wordView);

    /// Если слово не найдено, то добавляем его в таблицу
    if (it == freqMap.end ()) {
      /// Если не было больших букв то берем слово из памяти
      if (!hasUpper) {
        freqMap.emplace (wordView, 1); ///< Добавляем слово в таблицу
        words_.push_back (wordView);   ///< Добавляем слово в список слов
      } else {
        normalizedStorage_.push_back (std::move (tempWord)); ///< Перемещаем слово в хранилище
        std::string_view sv = normalizedStorage_.back ();    ///< Создаем string_view на сохраненную строку
        freqMap.emplace (sv, 1);                             ///< Добавляем слово в таблицу частот
        words_.push_back (sv);                               ///< Добавляем слово в список слов
      }
    } else {
      it->second++;                 ///< Увеличиваем счетчик частоты
      words_.push_back (it->first); ///< Добавляем уже сохраненное слово в список слов
    }
  }

  /// Очищаем частотный список
  frequency_.clear ();
  frequency_.reserve (freqMap.size ());

  /// Заполняем частотный список
  for (const auto &[word, count] : freqMap) {
    frequency_.emplace_back (word, count);
  }

  /// Сортируем частотный список сначала по убыванию частоты, затем по алфавиту
  std::sort (frequency_.begin (), frequency_.end (), [] (const auto &a, const auto &b) {
    if (a.second != b.second) {
      return a.second > b.second; ///< У кого частота больше, тот выше
    }
    return a.first < b.first; ///< Если частоты равна, сортируем по алфавиту
  });

  /// Заполняем список уникальных слов в порядке популярности
  uniqueWords_.clear ();
  uniqueWords_.reserve (frequency_.size ());
  for (const auto &item : frequency_) {
    uniqueWords_.push_back (item.first);
  }
}

/**
 * @brief Построение индекса слов
 * @return Индекс позиций всех слов
 */
ScanDocs::WordIndex ScanDocs::buildWordIndex () const {
  if (words_.empty ()) {
    return {};
  }

  /// Хэш таблица индекса слов, арг 1 слово, арг 2 позици слова в тексте
  std::unordered_map<std::string_view, std::vector<uint32_t>> indexMap;
  indexMap.reserve (uniqueWords_.size ()); // Резервируем по числу уникальных слов

  /// Проходим по всем словам и добавляем в таблицу только уникальные слова и уникальную позицию
  for (uint32_t i = 0; i < words_.size (); ++i) {
    indexMap[words_[i]].push_back (i);
  }

  WordIndex index; ///< Индекс слов
  index.reserve (indexMap.size ());

  /// Перемещаем элементы из хэш таблицы в вектор пар, чтобы потом отсортировать
  for (auto &[word, positions] : indexMap) {
    index.emplace_back (word, std::move (positions));
  }

  /// Сортируем по убыванию количества вхождений слова в тексте
  std::sort (index.begin (), index.end (), [] (const auto &a, const auto &b) {
    if (a.second.size () != b.second.size ()) {
      return a.second.size () > b.second.size (); /// У кого позиций больше, тот выше
    }
    return a.first < b.first; ///< Если позиций одинаково, сортируем по алфавиту
  });

  return index;
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
  fileSize_ = 0;
  isLoaded_ = false;
}

bool ScanDocs::loadFile (const std::filesystem::path &path) {
  std::error_code ec;                                     ///< Переменная для записи ошибки
  uintmax_t size = std::filesystem::file_size (path, ec); ///< Получаем размер файла

  /// Если возникла ошибка то возвращаем false
  if (ec)
    return false;

  /// Если файл пустой то устанавливаем флаг загрузки и возвращаем true
  if (size == 0) {
    fileSize_ = 0;
    isLoaded_ = true;
    return true;
  }

  fileSize_ = static_cast<size_t> (size); ///< Сохраняем размер файла

  HANDLE hFile = CreateFileW (path.c_str (),   // Имя файла
                              GENERIC_READ,    // Режим доступа (чтение)
                              FILE_SHARE_READ, // Совместный доступ (разрешаем читать)
                              nullptr,         // Атрибуты безопасности
                              OPEN_EXISTING,   // Открыть только существующий файл
                              FILE_ATTRIBUTE_NORMAL |
                                  FILE_FLAG_SEQUENTIAL_SCAN, // Флаги и оптимизация (нет спец свойств, читаем
                                                             // последовательно от начала и до конца)
                              nullptr);                      // Шаблонный файл

  /// Если файл не открыт, то возвращаем false
  if (hFile == INVALID_HANDLE_VALUE) {
    return false;
  }

  /// Создаем отобрадение файла в память
  HANDLE hMapping = CreateFileMappingW (hFile,         // Дескриптор открытого файла
                                        nullptr,       // Атрибуты безопасности
                                        PAGE_READONLY, // Защита страниц памяти
                                        0,             // 0 и 0 означают
                                        0,             // отображение файла целиком
                                        nullptr);      // Указатель на файл
  /// Если отображение не удалось, то закрываем файл и возвращаем false
  if (!hMapping) {
    CloseHandle (hFile);
    return false;
  }

  /// Получаем указатель на отображенную область памяти
  const char *pData = static_cast<const char *> (MapViewOfFile (hMapping, FILE_MAP_READ, 0, 0, 0));
  /// Если отображение не удалось, то закрываем файл и отображение и возвращаем false
  if (!pData) {
    CloseHandle (hMapping);
    CloseHandle (hFile);
    return false;
  }

  fileHandle_ = hFile;       // Сохраняем дескриптор файла
  mappingHandle_ = hMapping; // Сохраняем дескриптор отображения файла в память
  mappedData_ = pData;       // Сохраняем указатель на отображенную область памяти
  isLoaded_ = true;          // Устанавливаем флаг загрузки файла

  return true;
}

} // namespace core