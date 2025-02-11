#include "processing.hpp"

#include <cmath>
#include <range/v3/all.hpp>

double hann(size_t n, size_t N) {
  return 0.5 * (1 - std::cos(2 * M_PI * n / (N - 1)));
}
