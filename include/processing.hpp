#ifndef PROCESSING_HPP
#define PROCESSING_HPP

#include <complex>
#include <fftw3.h>
#include <range/v3/all.hpp>
#include <ranges>
#include <vector>

template <typename T>
concept DoubleRange = requires(T a) {
  requires std::convertible_to<std::ranges::range_value_t<T>, double>;
  requires std::ranges::range<T>;
};

std::vector<std::complex<double>> fft(DoubleRange auto &&input) {
  const size_t N = std::ranges::distance(input);
  if (N < 10) {
    return {};
  }
  const size_t N_out = N / 2 + 1;
  double *in = fftw_alloc_real(N);
  fftw_complex *out = fftw_alloc_complex(N_out);

  fftw_plan p = fftw_plan_dft_r2c_1d(N, in, out, FFTW_ESTIMATE);

  std::ranges::copy(input, in);

  fftw_execute(p);

  auto outComplex = std::ranges::subrange(out, out + N_out) |
                    std::views::transform([N](const auto &e) {
                      return std::complex<double>{e[0] / N, e[1] / N};
                    });
  auto normalized = outComplex | std::views::drop(1) |
                    std::views::transform([](const auto &e) { return 2. * e; });
  std::vector<std::complex<double>> result =
      ranges::views::concat(outComplex | ranges::views::take(1), normalized) |
      ranges::to_vector;

  fftw_destroy_plan(p);
  fftw_free(in);
  fftw_free(out);

  return result;
}

#endif
