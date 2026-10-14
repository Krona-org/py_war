#pragma once

#include <windows.h>
#include <string>

inline bool AppendSharedResult (const std::string &filePath, const std::string &textToAppend) {
  // 1. Открываем файл для добавления в конец, разрешая другим процессам доступ
  HANDLE hFile = CreateFileA (
      filePath.c_str (),
      FILE_APPEND_DATA,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,
      OPEN_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      NULL
  );

  if (hFile == INVALID_HANDLE_VALUE) {
    return false;
  }

  // 2. Блокируем весь файл эксклюзивно.
  // Если файл заблокирован другим процессом, LockFileEx ждет своей очереди без холостого хода
  OVERLAPPED ov = {0};
  LockFileEx (hFile, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &ov);

  // 3. Записываем данные
  DWORD bytesWritten = 0;
  WriteFile (hFile, textToAppend.c_str (), static_cast<DWORD> (textToAppend.size ()), &bytesWritten, NULL);

  // 4. Снимаем блокировку, пропуская следующий процесс из очереди
  UnlockFileEx (hFile, 0, MAXDWORD, MAXDWORD, &ov);
  CloseHandle (hFile);
  return true;
}
