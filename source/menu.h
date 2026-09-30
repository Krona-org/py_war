#pragma once

#include "scandocs.h"

#include <chrono>
#include <filesystem>
#include <string>

namespace app {

/// @brief Вызов системного диалогового окна выбора файла Windows
/// @return Путь к выбранному файлу или пустая строка при отмене
std::string OpenFileDialog ();

/// @brief Класс циклического меню и генерации отчетов по документу
class DocumentMenu {
public:
  /// @brief Конструктор
  /// @param filePath Путь к анализируемому файлу
  explicit DocumentMenu (const std::string &filePath = "");

  /// @brief Загрузка и анализ нового файла с замером времени
  /// @param filePath Путь к файлу
  /// @return true, если файл успешно загружен и обработан
  bool loadFile (const std::string &filePath);

  /// @brief Проверка статуса загрузки документа
  bool isLoaded () const noexcept;

  /// @brief Запуск интерактивного циклического меню
  void runInteractiveMenu ();

  /// @brief Сохранение отчета в файл (без потокового вывода на экран)
  /// @param outputPath Путь к файлу для сохранения (по умолчанию "report.txt")
  /// @param topWordsLimit Количество слов для топа частоты (по умолчанию 10)
  /// @param topPositionsLimit Количество слов для топа индексации (по умолчанию 10)
  /// @return true в случае успешной записи
  bool saveReportToFile (const std::string &outputPath = "report.txt", size_t topWordsLimit = 10,
                         size_t topPositionsLimit = 10);

  /// @brief Сохранение отчета в файл для конкретного режима (--mode 1, 2 или 0)
  /// @param mode 1: частота слов, 2: количество встреч и позиций слов, 0: полный отчет
  /// @param outputPath Имя файла результата (по умолчанию "result.txt")
  /// @param limit Количество слов для вывода (по умолчанию 10)
  /// @return true в случае успешной записи
  bool saveModeReport (int mode, const std::string &outputPath = "result.txt", size_t limit = 10);

  /// @brief Печать заголовка документа (имя файла, размер, время обработки)
  void printHeader () const;

  /// @brief Пункт 1: Показать количество уникальных слов и топ самых частых
  /// @param limit Количество слов для вывода (по умолчанию 10)
  void showUniqueWords (size_t limit = 10) const;

  /// @brief Пункт 2: Показать топ индексации позиций слов
  /// @param limit Количество слов для вывода (по умолчанию 10)
  /// @param maxPositionsPerWord Максимальное число позиций для вывода у каждого слова
  void showWordPositions (size_t limit = 10, size_t maxPositionsPerWord = 15) const;

  /// @brief Получение пути к текущему файлу
  const std::string &getFilePath () const noexcept { return filePath_; }

  /// @brief Получение времени обработки документа в миллисекундах
  double getProcessingTimeMs () const noexcept { return processingTimeMs_; }

private:
  std::string filePath_;
  core::ScanDocs scanDocs_;
  double processingTimeMs_ {0.0};
};

} // namespace app
