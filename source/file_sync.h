#pragma once

#include <filesystem>
#include <string>
#include <Windows.h>

namespace sync {

/// @brief Класс для межпроцессной синхронизации записи в файл
class FileSynchronizer {
public:
  /// @brief Конструктор синхронизатора для указанного файла
  /// @param targetFilePath Путь к файлу, в который будет производиться запись
  explicit FileSynchronizer (const std::filesystem::path &targetFilePath);

  /// @brief Деструктор: освобождает дескрипторы и счетчики
  ~FileSynchronizer ();

  // Запрет копирования
  FileSynchronizer (const FileSynchronizer &) = delete;
  FileSynchronizer &operator= (const FileSynchronizer &) = delete;

  /// @brief Безопасная запись данных в файл
  /// Если процесс первый в группе - файл перезаписывается (trunc).
  /// Если файл уже занят другим процессом - процесс ждет в очереди и дописывает данные ниже (app).
  /// @param content Текст для записи
  /// @return true в случае успешной записи
  bool write (const std::string &content);

  /// @brief Статический метод безопасной записи в файл
  /// @param targetFilePath Путь к файлу
  /// @param content Текст для записи
  /// @return true в случае успеха
  static bool writeSynchronized (const std::filesystem::path &targetFilePath, const std::string &content);

private:
  std::filesystem::path targetPath_;
  std::string baseSyncName_;
  HANDLE hMutex_ {NULL};
  HANDLE hMap_ {NULL};
  void *sharedMem_ {nullptr};

  /// @brief Внутренняя структура состояния в разделяемой памяти Windows
  struct SharedState {
    LONG activeCount;        ///< Количество процессов, работающих с данным файлом
    LONG hasWrittenFirst;    ///< Флаг: 0 - файл еще не записан первым процессом, 1 - первый процесс уже записал
    ULONGLONG lastWriteTime; ///< Время последней записи (GetTickCount64)
  };

  void initSync ();
  void cleanupSync ();
  static std::string makeSafeIpcName (const std::filesystem::path &p);
};

} // namespace sync
