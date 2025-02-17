#ifndef PROCESSING_HPP
#define PROCESSING_HPP

#include <complex>
#include <concepts>
#include <fftw3.h>
#include <functional>
#include <mutex>
#include <range/v3/all.hpp>
#include <ranges>
#include <unordered_map>
#include <vector>

inline constexpr double OVERLAP = 0.5;
double hann(size_t n, size_t N);
double hamming(size_t n, size_t N);
double blackman(size_t n, size_t N);

template <typename T>
concept DoubleRange = requires(T a) {
  requires std::convertible_to<std::ranges::range_value_t<T>, double>;
  requires std::ranges::range<T>;
};

using WindowFunction = std::function<double(size_t, size_t)>;
inline constexpr std::array AVAILABLE_WINDOWS{hann, hamming, blackman};
inline const std::unordered_map<std::string, WindowFunction> WINDOW_MAP{
    {"Hann", hann}, {"Hamming", hamming}, {"Blackman", blackman}};

auto applyWindow(DoubleRange auto &&in, WindowFunction f = hann) {
  size_t N = ranges::distance(in);
  return ranges::views::enumerate(in) |
         ranges::views::transform([N, f](auto &&p) {
           auto &&[i, e] = std::forward<decltype(p)>(p);
           return f(i, N) * e;
         });
}

std::vector<std::complex<double>> fft(DoubleRange auto &&input,
                                      std::mutex *lock = nullptr) {

  const size_t N = std::ranges::distance(input);
  if (N < 10) {
    return {};
  }
  const size_t N_out = N / 2 + 1;
  double *in = fftw_alloc_real(N);
  fftw_complex *out = fftw_alloc_complex(N_out);
  fftw_plan p;

  if (lock) {
    std::unique_lock temp{*lock};
    p = fftw_plan_dft_r2c_1d(N, in, out, FFTW_ESTIMATE);
  } else {
    p = fftw_plan_dft_r2c_1d(N, in, out, FFTW_ESTIMATE);
  }

  std::ranges::copy(input, in);

  fftw_execute(p);

  auto outComplex = std::span(out, N_out) |
                    std::views::transform([N](auto &&e) {
                      return std::complex<double>{e[0] / N, e[1] / N} * 2.;
                    }) |
                    ranges::to_vector;
  outComplex[0] /= 2;
  if (N % 2 == 0) {
    outComplex[N / 2] /= 2;
  }

  if (lock) {
    std::unique_lock temp{*lock};
    fftw_destroy_plan(p);
  } else {
    fftw_destroy_plan(p);
  }
  fftw_free(in);
  fftw_free(out);

  return outComplex;
}

std::vector<double> welch(DoubleRange auto &&dataA, DoubleRange auto &&dataB,
                          size_t windowSize = 1024, WindowFunction windowFn = hann) {
  namespace rv = ranges::views;
  const size_t N = ranges::distance(dataA);
  if (N < 10 || ranges::distance(dataB) != N) {
    return {};
  }

  size_t stride = windowSize * OVERLAP;
  auto limit = N - windowSize + stride;

  size_t count = 0;
  std::vector<std::complex<double>> total(windowSize / 2 + 1, {0., 0.});

  if (N <= windowSize) {
    auto a = fft(applyWindow(dataA, windowFn));
    auto b = fft(applyWindow(dataB, windowFn));
    auto tnsf = rv::zip(a, b) | rv::transform([](auto &&p) {
                  return std::pow(p.first / p.second, 2);
                });
    total = tnsf | ranges::to_vector;
    count = 1;
  } else {

    std::mutex lock;

#pragma omp parallel for
    for (int left = 0; left < limit; left += stride) {
      int right = left + windowSize;
      auto a = dataA | rv::slice(left, right > N ? static_cast<int>(N) : right);
      auto b = dataB | rv::slice(left, right > N ? static_cast<int>(N) : right);

      size_t pad = right > N ? right - N : 0;
      auto padrng = rv::repeat_n(0., pad);
      auto aPadded = rv::concat(a, padrng);
      auto bPadded = rv::concat(b, padrng);

      auto aHanned = applyWindow(aPadded, windowFn);
      auto bHanned = applyWindow(bPadded, windowFn);

      auto aTrans = fft(aHanned, &lock);
      auto bTrans = fft(bHanned, &lock);

      auto tnsf = rv::zip(aTrans, bTrans) | rv::transform([](auto &&p) {
                    auto &&[a, b] = std::forward<decltype(p)>(p);
                    auto temp = a / b;
                    return temp * temp;
                  });

#pragma omp critical
      {
        ranges::for_each(rv::enumerate(tnsf), [&total](auto &&p) {
          auto &&[i, e] = std::forward<decltype(p)>(p);
          total[i] += e;
        });
      }
#pragma omp atomic
      ++count;
    }
  }

  auto ret =
      total | rv::transform([count](auto &&e) { return e / (double)count; }) |
      rv::transform([](auto &&e) { return 10 * std::log10(std::abs(e)); }) |
      ranges::to_vector;

  return ret;
}

#endif
