#include "findlabel.h"

#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <iostream>

#include <algorithm>
#include <vector>

// --- Задание 3.a: Проверка на простоту и возведение простых в квадрат ---
bool isPrime (int n) {
  if (n <= 1)
    return false;
  if (n <= 3)
    return true;
  if (n % 2 == 0 || n % 3 == 0)
    return false;
  for (int i = 5; i * i <= n; i += 6) {
    if (n % i == 0 || n % (i + 2) == 0)
      return false;
  }
  return true;
}

// Модификация через std::transform
void squarePrimes (std::vector<int> &vec) {
  std::transform (vec.begin (), vec.end (), vec.begin (), [] (int x) {
    return isPrime (x) ? (x * x) : x;
  });
}

// --- Задание 3.b: Нечетные по возрастанию, затем четные по убыванию ---
// Сортировка через std::sort с предикатом
void sortOddAscEvenDesc (std::vector<int> &vec) {
  std::sort (vec.begin (), vec.end (), [] (int a, int b) {
    bool a_odd = (a % 2 != 0);
    bool b_odd = (b % 2 != 0);
    if (a_odd != b_odd)
      return a_odd; // нечетные идут первыми
    if (a_odd)
      return a < b; // нечетные по возрастанию
    return a > b;   // четные по убыванию
  });
}

// --- Задание 3.c: Поиск уникальных чисел в диапазоне [min_val, max_val] ---
// Фильтрация через std::copy_if и удаление дубликатов через std::sort + std::unique
std::vector<int> findUniqueInRange (const std::vector<int> &vec, int min_val, int max_val) {
  std::vector<int> result;
  std::copy_if (vec.begin (), vec.end (), std::back_inserter (result), [min_val, max_val] (int x) {
    return x >= min_val && x <= max_val;
  });

  std::sort (result.begin (), result.end ());
  auto last = std::unique (result.begin (), result.end ());
  result.erase (last, result.end ());

  return result;
}

int main () {
  SetConsoleCP (65001);
  SetConsoleOutputCP (65001);
  std::ios::sync_with_stdio (false); // ускоряет вывод std::cout в разы

  auto start = std::chrono::steady_clock::now ();
  std::filesystem::path path_ = std::filesystem::path ("voina_mir") / "Vojna i mir. Tom 1.txt";

  core::FindLabel findLabel (path_);
  std::vector<std::string_view> words = findLabel.getUniqueWords ();

  std::cout << "---Задание 1 (первые 10 слов)---\n";
  size_t t1_count = 0;
  for (const auto &[word, count] : findLabel.getFrequency ()) {
    std::cout << word << ' ' << count << '\n';
    if (++t1_count >= 10)
      break;
  }

  std::cout << "\n---Задание 2 (первые 10 позиций)---\n";
  findLabel.printWordIndex (std::cout, 10);

  std::cout << "\n---Задание 3---\n";

  // 3.a Демонстрация:
  std::vector<int> vecA = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  std::cout << "3.a) Исходный: ";
  for (int x : vecA) std::cout << x << ' ';
  squarePrimes (vecA);
  std::cout << "\n     После возведения простых в квадрат: ";
  for (int x : vecA) std::cout << x << ' ';
  std::cout << "\n\n";

  // 3.b Демонстрация:
  std::vector<int> vecB = {5, 2, 8, 1, 9, 4, 3, 6, 7, 10};
  std::cout << "3.b) Исходный: ";
  for (int x : vecB) std::cout << x << ' ';
  sortOddAscEvenDesc (vecB);
  std::cout << "\n     Нечетные (возр.), затем четные (убыв.): ";
  for (int x : vecB) std::cout << x << ' ';
  std::cout << "\n\n";

  // 3.c Демонстрация:
  std::vector<int> vecC = {15, 3, 7, 20, 7, 10, 3, 5, 25, 12, 10, 7};
  int min_range = 5, max_range = 15;
  std::cout << "3.c) Исходный: ";
  for (int x : vecC) std::cout << x << ' ';
  std::cout << "\n     Диапазон [" << min_range << ", " << max_range << "]";
  std::vector<int> uniqueInRange = findUniqueInRange (vecC, min_range, max_range);
  std::cout << "\n     Уникальные элементы в диапазоне: ";
  for (int x : uniqueInRange) std::cout << x << ' ';
  std::cout << "\n\n";

  auto end = std::chrono::steady_clock::now ();
  auto result = std::chrono::duration_cast<std::chrono::milliseconds> (end - start);
  std::cout << "Время работы программы: " << result << '\n';
  return 0;
}