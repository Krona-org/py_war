#include "scandocs.h"

#include <Windows.h>
#include <iostream>

std::string OpenFileDialog () {
  OPENFILENAME openFileName;
  char sizeFile[260] = {0};

  ZeroMemory (&openFileName, sizeof (openFileName));
  openFileName.lStructSize = sizeof (openFileName);
  openFileName.hwndOwner = NULL;
  openFileName.lpstrFile = sizeFile;
  openFileName.nMaxFile = sizeof (sizeFile);
  openFileName.lpstrFilter = "All Files\0*.*\0Text Files\0*.TXT\0";
  openFileName.nFilterIndex = 1;
  openFileName.lpstrFileTitle = NULL;
  openFileName.nMaxFileTitle = 0;
  openFileName.lpstrInitialDir = NULL;
  openFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileNameA (&openFileName) == TRUE) {
    return std::string (openFileName.lpstrFile);
  }
  return "";
}

int main () {
  SetConsoleCP (65001);
  SetConsoleOutputCP (65001);
  std::ios::sync_with_stdio (false); // ускоряет вывод std::cout

  core::ScanDocs scanDocs (OpenFileDialog ());

  return 0;
}
