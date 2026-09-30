#include "file_sync.h"

#include <fstream>
#include <iostream>

namespace sync {

std::string FileSynchronizer::makeSafeIpcName (const std::filesystem::path &p) {
  std::error_code ec;
  std::string absPath = std::filesystem::absolute (p, ec).lexically_normal ().string ();

  // Формируем имя для объекта ядра Windows (префикс Local\ для текущей пользовательской сессии)
  std::string safeName = "Local\\PyWarSync_";
  for (char ch : absPath) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
      safeName += ch;
    } else {
      safeName += '_';
    }
  }

  // Добавляем хэш полного пути для гарантии уникальности и предотвращения превышения длины
  size_t pathHash = std::hash<std::string>{}(absPath);
  safeName += "_" + std::to_string (pathHash);

  return safeName;
}

FileSynchronizer::FileSynchronizer (const std::filesystem::path &targetFilePath)
    : targetPath_ (targetFilePath)
    , baseSyncName_ (makeSafeIpcName (targetFilePath)) {
  initSync ();
}

FileSynchronizer::~FileSynchronizer () {
  cleanupSync ();
}

void FileSynchronizer::initSync () {
  std::string mapName = baseSyncName_ + "_map";
  std::string mutexName = baseSyncName_ + "_mtx";

  // 1. Создаем или открываем разделяемую память для межпроцессного счетчика
  hMap_ = CreateFileMappingA (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                              sizeof (SharedState), mapName.c_str ());

  if (hMap_ != NULL) {
    sharedMem_ = MapViewOfFile (hMap_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof (SharedState));
    if (sharedMem_ != nullptr) {
      auto *state = static_cast<SharedState *> (sharedMem_);
      // Атомарно увеличиваем счетчик активных процессов для этого файла
      InterlockedIncrement (&state->activeCount);
    }
  }

  // 2. Создаем или открываем именованный мьютекс Windows для очереди записи
  hMutex_ = CreateMutexA (NULL, FALSE, mutexName.c_str ());
}

void FileSynchronizer::cleanupSync () {
  if (sharedMem_ != nullptr) {
    auto *state = static_cast<SharedState *> (sharedMem_);
    LONG remaining = InterlockedDecrement (&state->activeCount);
    // Если все параллельные процессы завершили работу - сбрасываем флаг первой записи
    if (remaining <= 0) {
      state->hasWrittenFirst = 0;
    }
    UnmapViewOfFile (sharedMem_);
    sharedMem_ = nullptr;
  }

  if (hMap_ != NULL) {
    CloseHandle (hMap_);
    hMap_ = NULL;
  }

  if (hMutex_ != NULL) {
    CloseHandle (hMutex_);
    hMutex_ = NULL;
  }
}

bool FileSynchronizer::write (const std::string &content) {
  if (hMutex_ == NULL) {
    // Резервный вариант, если мьютекс не создался
    std::ofstream out (targetPath_, std::ios::out | std::ios::trunc);
    if (!out.is_open ())
      return false;
    out << content;
    return out.good ();
  }

  // Встаем в очередь на доступ к файлу и ждем освобождения мьютекса
  DWORD waitResult = WaitForSingleObject (hMutex_, INFINITE);
  if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_ABANDONED) {
    return false;
  }

  ULONGLONG now = GetTickCount64 ();
  bool isFirstProcess = true;
  auto *state = static_cast<SharedState *> (sharedMem_);

  if (state != nullptr) {
    if (state->hasWrittenFirst != 0) {
      isFirstProcess = false; // Первый процесс уже выполнил запись, остальные дописывают
    }
  }

  // Проверяем временное окно для группы процессов (если запуски происходили с небольшим смещением)
  std::filesystem::path syncMetaPath = targetPath_.string () + ".sync";
  if (isFirstProcess && std::filesystem::exists (targetPath_)) {
    std::ifstream syncIn (syncMetaPath);
    if (syncIn.is_open ()) {
      ULONGLONG prevTime = 0;
      if (syncIn >> prevTime) {
        // Если запись была менее 4 секунд назад — считаем процессом из той же группы
        if (now >= prevTime && (now - prevTime) < 4000) {
          isFirstProcess = false;
        }
      }
    }
  }

  bool writeSuccess = false;
  try {
    // Первый процесс перезаписывает файл (trunc), последующие параллельные - дописывают (app)
    std::ios_base::openmode mode = std::ios::out | (isFirstProcess ? std::ios::trunc : std::ios::app);
    std::ofstream out (targetPath_, mode);

    if (out.is_open ()) {
      if (!isFirstProcess) {
        // Добавляем разделитель между записями параллельных процессов
        out << "\n\n";
      }
      out << content;
      out.flush ();
      writeSuccess = out.good ();
      out.close ();
    }
  } catch (...) {
    writeSuccess = false;
  }

  if (writeSuccess) {
    if (state != nullptr) {
      state->hasWrittenFirst = 1;
      state->lastWriteTime = now;
    }
    // Сохраняем временную метку последней записи
    std::ofstream syncOut (syncMetaPath, std::ios::out | std::ios::trunc);
    if (syncOut.is_open ()) {
      syncOut << now;
    }
  }

  // Освобождаем мьютекс для следующего процесса в очереди
  ReleaseMutex (hMutex_);

  return writeSuccess;
}

bool FileSynchronizer::writeSynchronized (const std::filesystem::path &targetFilePath,
                                          const std::string &content) {
  FileSynchronizer synchronizer (targetFilePath);
  return synchronizer.write (content);
}

} // namespace sync
