#include "processing.hpp"

#include <cmath>
#include <numbers>
#include <range/v3/all.hpp>

using namespace std::numbers;
double hann(size_t n, size_t N) {
  return 0.5 * (1 - std::cos(2 * pi * n / (N - 1)));
}

double hamming(size_t n, size_t N) {
  return 0.54 - 0.46 * std::cos(2 * pi * n / (N - 1));
}

double blackman(size_t n, size_t N) {
  return 0.42 - 0.5 * std::cos(2 * pi * n / (N - 1)) +
         0.08 * std::cos(4 * pi * n / (N - 1));
}
