#include "processing.hpp"
#include "fftw3.h"

std::vector<std::complex<double>> fft(std::vector<double> &input) {

  const size_t N = input.size();
  const size_t N_out = N / 2 + 1;
  double *in = fftw_alloc_real(N);
  fftw_complex *out = fftw_alloc_complex(N_out);

  fftw_plan p = fftw_plan_dft_r2c_1d(N, in, out, FFTW_ESTIMATE);

  for (size_t i = 0; i < N; ++i) {
    in[i] = input[i];
  }

  fftw_execute(p);

  std::vector<std::complex<double>> result(N_out);
  for (size_t i = 0; i < N_out; ++i) {
    result[i] = std::complex<double>(out[i][0] / N, out[i][1] / N);
  }

  for (size_t i = 1; i < N_out - 1; ++i) {
    result[i] *= 2;
  }

  fftw_destroy_plan(p);
  fftw_free(in);
  fftw_free(out);

  return result;
}
