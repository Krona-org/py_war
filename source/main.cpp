#include "findlabel.h"

#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <utility>

int main () {
  SetConsoleCP (65001);
  SetConsoleOutputCP (65001);

  // для максимальной производительности возьмем среднее количество букв 5

  std::filesystem::path path_ = std::filesystem::path ("voina_mir") / "Vojna i mir. Tom 1.txt";

  core::FindLabel findLabel (path_);
  std::vector<std::string_view> words = findLabel.getUniqueWords ();

  std::cout << " d";
  return 0;
}