#include "pico.hpp"
#include "mpsc.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <libps2000/ps2000.h>
#include <range/v3/all.hpp>

#define TRUE 1
#define FALSE 0

namespace {
std::optional<mpsc::Send<StreamResult>> streamSender;
enPS2000Range voltageRangeGlob = DEFAULT_VOLTAGE_RANGE;
std::mutex globalLock;

double toVolts(enPS2000Range range);

auto callback = [](int16_t **overviewBuffers, int16_t overflow,
                   uint32_t triggeredAt, int16_t triggered, int16_t auto_stop,
                   uint32_t nValues) {
  std::unique_lock temp{globalLock};
  if (!streamSender.has_value()) {
    return;
  }
  auto f = ranges::views::transform([](const int16_t &e) -> double {
    return (double)e / (double)PS2000_MAX_VALUE * toVolts(voltageRangeGlob);
  });
  auto rangeA =
      ranges::make_subrange(overviewBuffers[0], overviewBuffers[0] + nValues) |
      f;
  auto rangeB =
      ranges::make_subrange(overviewBuffers[2], overviewBuffers[2] + nValues) |
      f;

  streamSender.value().send(
      StreamResult{rangeA | ranges::to_vector, rangeB | ranges::to_vector});
};

double toVolts(enPS2000Range range) {
  switch (range) {
  case PS2000_50MV:
    return 50. / 1000.;
  case PS2000_100MV:
    return 100. / 1000.;
  case PS2000_200MV:
    return 200. / 1000.;
  case PS2000_500MV:
    return 500. / 1000.;
  case PS2000_1V:
    return 1.;
  case PS2000_2V:
    return 2.;
  case PS2000_5V:
    return 5.;
  case PS2000_10V:
    return 10.;
  case PS2000_20V:
    return 20.;
  default:
    return 0.;
  }
}
} // namespace

std::array<uint8_t, AWG_BUF_SIZE> getNoiseWaveform() {
  std::array<uint8_t, AWG_BUF_SIZE> buffer;
  srand(0);
  for (auto &point : buffer) {
    point = rand() % UINT8_MAX;
  }
  return buffer;
}

Scope &Scope::getInstance() {
  static Scope scope;
  return scope;
}

Scope::Scope() {}
Scope::~Scope() {
  if (isStreaming()) {
    stopStream();
  }

  if (isOpen()) {
    ps2000_close_unit(handle);
  }
}

bool Scope::openScope() {
  auto res = ps2000_open_unit();
  if (res <= 0) {
    return false;
  }
  handle = res;
  open = true;
  return true;
}

bool Scope::isOpen() {
  if (open) {
    auto res = ps2000PingUnit(handle);
    if (res == 0) {
      open = false;
    }
  }
  return open;
}

bool Scope::isStreaming() {
  // if (streaming && !isOpen()) {
  //   stopStream();
  // }
  return streaming;
}
bool Scope::isGenerating() {
  // if (generating && !isOpen()) {
  //   generating = false;
  // }
  return generating;
}

void Scope::setVoltageRange(enPS2000Range range) {
  auto prevRange = voltageRange;
  voltageRange = range;
  if (streaming && prevRange != range) {
    restartStream();
  }
}

void Scope::setStreamingMode(bool dc) {
  auto prev = this->dc;
  this->dc = dc;
  if (streaming && prev != dc) {
    restartStream();
  }
}

void Scope::restartStream(bool settingsChanged) {
  std::unique_lock temp{globalLock};
  if (!streamSender.has_value()) {
    return;
  }
  if (streaming) {
    stopStream();
  }

  if (settingsChanged) {
    ps2000_set_channel(handle, PS2000_CHANNEL_A, TRUE, dc, voltageRange);
    ps2000_set_channel(handle, PS2000_CHANNEL_B, TRUE, dc, voltageRange);
    ps2000_set_trigger(handle, PS2000_NONE, 0, PS2000_RISING, 0, 0);
    voltageRangeGlob = voltageRange;
  }

  ps2000_run_streaming_ns(handle, SAMPLE_INTERVAL, TIME_UNITS, SAMPLE_RATE * 10,
                          FALSE, 1, 1e6);
  streaming = true;
  auto &streaming = this->streaming;
  auto f = [handle = handle, &streaming]() mutable {
    while (streaming) {
      ps2000_get_streaming_last_values(handle, callback);
    }
  };
  streamTask = std::thread{f};
}

std::optional<mpsc::Recv<StreamResult>> Scope::startStream() {
  if (streaming) {
    stopStream();
  }
  ps2000_set_channel(handle, PS2000_CHANNEL_A, TRUE, dc, voltageRange);
  ps2000_set_channel(handle, PS2000_CHANNEL_B, TRUE, dc, voltageRange);
  ps2000_set_trigger(handle, PS2000_NONE, 0, PS2000_RISING, 0, 0);
  auto started = ps2000_run_streaming_ns(handle, SAMPLE_INTERVAL, TIME_UNITS,
                                         SAMPLE_RATE * 10., FALSE, 1, 1e6);
  if (!started) {
    return std::nullopt;
  }

  auto [send, recv] = mpsc::make<StreamResult>();
  streaming = true;

  {
    std::unique_lock temp{globalLock};
    voltageRangeGlob = voltageRange;
    streamSender.emplace(std::move(send));
  }

  auto &streaming = this->streaming;
  auto f = [handle = handle, &streaming]() mutable {
    while (streaming) {
      ps2000_get_streaming_last_values(handle, callback);
    }
  };

  streamTask = std::thread{f};

  return {std::move(recv)};
}

void Scope::stopStream() {
  if (streaming) {
    streaming = false;
    streamTask.join();
    ps2000_stop(handle);
  }
}

bool Scope::startNoise(double pkToPkV) {
  bool restartStream = false;
  if (streaming) {
    stopStream();
    restartStream = true;
  }
  uint8_t buf[NOISE_WAVEFORM.size()];
  std::copy(NOISE_WAVEFORM.cbegin(), NOISE_WAVEFORM.cend(), buf);
  auto success = ps2000_set_sig_gen_arbitrary(
      handle, 0, pkToPkV * 1e6, DELTA_PHASE, DELTA_PHASE, 0, 1, buf,
      NOISE_WAVEFORM.size(), PS2000_UP, 0);
  if (restartStream) {
    this->restartStream(false);
  }
  if (success) {
    printf("Success\n");
    generating = true;
    return true;
  }
  return false;
}

bool Scope::startFreqSweep(double start, double end, double pkToPkV,
                           uint32_t sweeps, double sweepDuration,
                           PS2000_SWEEP_TYPE sweepType) {
  bool restartStream = false;
  if (streaming) {
    stopStream();
    restartStream = true;
  }

  uint32_t pkToPkMicroV = pkToPkV * 1e6;
  double range = end - start;
  double incrementsPerSweep = sweepDuration / DWELL_TIME;
  float increment = range / incrementsPerSweep;
  auto success = ps2000_set_sig_gen_built_in(handle, 0, pkToPkMicroV,
                                             PS2000_SINE, start, end, increment,
                                             DWELL_TIME, sweepType, sweeps);

  if (restartStream) {
    this->restartStream(false);
  }
  if (success) {
    generating = true;
    return true;
  }
  return false;
}

void Scope::stopSigGen() {
  bool restartStream = false;
  if (streaming) {
    stopStream();
    restartStream = true;
  }
  ps2000_set_sig_gen_built_in(handle, 0, 0, PS2000_DC_VOLTAGE, 0, 0, 0, 0,
                              PS2000_UP, 0);
  if (restartStream) {
    this->restartStream(false);
  }
  generating = false;
}
