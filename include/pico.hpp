#ifndef PICO_HPP
#define PICO_HPP

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include "libps2000/ps2000.h"

#define DEFAULT_VOLTAGE_RANGE PS2000_10V
#define SAMPLE_INTERVAL 20
#define TIME_UNITS PS2000_US

constexpr double DWELL_TIME = 0.02;

constexpr double timeUnitToSecs(enPS2000TimeUnits unit) {
  switch (unit) {
  case PS2000_FS:
    return 1.0 / 1e15;
  case PS2000_PS:
    return 1.0 / 1e12;
  case PS2000_NS:
    return 1.0 / 1e9;
  case PS2000_US:
    return 1.0 / 1e6;
  case PS2000_MS:
    return 1.0 / 1000;
  case PS2000_S:
    return 1.0;
  default:
    return 1.0;
  }
}

struct Channel {
  std::vector<double> data;

  // Data needs to be accessed across threads
  std::mutex dataLock;
};

class Scope {
  int16_t handle = 0;

  bool open = false;
  std::atomic<bool> streaming = false;
  bool generating = false;
  std::thread streamTask;
  bool dc = false;

  enPS2000Range voltageRange = DEFAULT_VOLTAGE_RANGE;
  Channel channelA;
  Channel channelB;

public:
  Scope();
  ~Scope();

  void openScope();
  bool isOpen() const;
  bool isStreaming() const;
  bool isGenerating() const;

  constexpr double getDeltaTime() const {
    double unitInSecs = timeUnitToSecs(TIME_UNITS);
    return SAMPLE_INTERVAL * unitInSecs;
  }

  constexpr double getSampleRate() const { return 1. / getDeltaTime(); }

  void setVoltageRange(enPS2000Range range);
  void setStreamingMode(bool dc);
  bool startStream();
  void stopStream();

  void startNoise();
  bool startFreqSweep(double start, double end, double pkToPkV,
                           uint32_t sweeps, PS2000_SWEEP_TYPE sweepType, double duration);
  void stopSigGen();

  const Channel &getChannelA() const;
  const Channel &getChannelB() const;
  void lockChannels();
  void unlockChannels();

  void clearData();

  // Scope object cannot be copied
  Scope(const Scope &other) = delete;
  // Scope object cannot be moved
  Scope(Scope &&other) = delete;
};

#endif
