
#include <iostream>
#include <vector>
#include <algorithm>

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
  std::transform (vec.begin (), vec.end (), vec.begin (), [] (int x) { return isPrime (x) ? (x * x) : x; });
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
  std::copy_if (vec.begin (), vec.end (), std::back_inserter (result),
                [min_val, max_val] (int x) { return x >= min_val && x <= max_val; });

  std::sort (result.begin (), result.end ());
  auto last = std::unique (result.begin (), result.end ());
  result.erase (last, result.end ());

  return result;
}