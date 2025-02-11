#ifndef PROCESSING_HPP
#define PROCESSING_HPP

#include <complex>
#include <fftw3.h>
#include <range/v3/all.hpp>
#include <ranges>
#include <vector>

inline constexpr double OVERLAP = 0.5;
double hann(size_t n, size_t N);

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
                    std::views::transform([N](auto &&e) {
                      return std::complex<double>{e[0] / N, e[1] / N} * 2.;
                    }) |
                    ranges::to_vector;
  outComplex[0] /= 2;
  if (N % 2 == 0) {
    outComplex[N / 2] /= 2;
  }

  fftw_destroy_plan(p);
  fftw_free(in);
  fftw_free(out);

  return outComplex;
}

std::vector<double> welch(DoubleRange auto &&dataA, DoubleRange auto &&dataB,
                          size_t windowSize = 1024) {
  namespace rv = ranges::views;
  const size_t N = ranges::distance(dataA);
  if (N < 10 || ranges::distance(dataB) != N) {
    return {};
  }

  size_t stride = windowSize * OVERLAP;

  size_t count = 0;
  std::vector<std::complex<double>> total(windowSize / 2 + 1, {0., 0.});
  for (size_t left = 0; left + windowSize - stride < N; left += stride) {
    size_t right = left + windowSize;
    auto a = dataA | rv::slice(left, right > N ? N : right);
    auto b = dataB | rv::slice(left, right > N ? N : right);

    size_t pad = right > N ? right - N : 0;
    auto padrng = rv::repeat_n(0., pad);
    auto aPadded = rv::concat(a, padrng);
    auto bPadded = rv::concat(b, padrng);

    auto aHanned =
        rv::enumerate(aPadded) | rv::transform([windowSize](auto &&p) {
          auto &&[i, e] = std::forward<decltype(p)>(p);
          return e * hann(i, windowSize);
        });
    auto bHanned =
        rv::enumerate(bPadded) | rv::transform([windowSize](auto &&p) {
          auto &&[i, e] = std::forward<decltype(p)>(p);
          return e * hann(i, windowSize);
        });

    auto aTrans = fft(aHanned);
    auto bTrans = fft(bHanned);

    auto tnsf = rv::zip(aTrans, bTrans) | rv::transform([](auto &&p) {
                  auto &&[a, b] = std::forward<decltype(p)>(p);
                  auto temp = a / b;
                  return temp * temp;
                });

    ranges::for_each(rv::enumerate(tnsf), [&total](auto &&p) {
      auto &&[i, e] = std::forward<decltype(p)>(p);
      total[i] += e;
    });
    ++count;
  }

  auto ret =
      total | rv::transform([count](auto &&e) { return e / (double)count; }) |
      rv::transform([](auto &&e) { return 10 * std::log10(std::abs(e)); }) |
      ranges::to_vector;

  return ret;
}

#endif
