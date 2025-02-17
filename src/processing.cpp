#include "processing.hpp"

#include <cmath>
#include <range/v3/all.hpp>
#include <numbers>

double hann(size_t n, size_t N) {
  return 0.5 * (1 - std::cos(2 * std::numbers::pi * n / (N - 1)));
}

double hamming(size_t n, size_t N) {
  return 0.54 - 0.46 * std::cos(2 * std::numbers::pi * n / (N - 1));
}
