#ifndef PICO_HPP
#define PICO_HPP

#include <mutex>
#include <vector>
#include "ps2000.h"

#define DEFAULT_VOLTAGE_RANGE PS2000_10V


struct Channel {
  std::vector<double> data;

  // Data needs to be accessed across threads
  std::mutex dataLock;
};

class Scope {
  int16_t handle = 0;

  bool open = false;
  std::atomic<bool> streaming = false;
  std::thread streamTask;
  bool dc = false;

  enPS2000Range voltageRange = DEFAULT_VOLTAGE_RANGE;
  Channel channelA;
  Channel channelB;

  Scope();
  ~Scope();

  void openScope();
  bool isOpen() const;
  constexpr double getDeltaTime() const;
  constexpr double getSampleRate() const;

  void setVoltageRange(enPS2000Range range);
  void setStreamingMode(bool dc);
  bool startStream();
  void stopStream();

  void clearData();
   

  // Scope object cannot be copied
  Scope(const Scope &other) = delete;
  // Scope object cannot be moved
  Scope(Scope &&other) = delete;
};

#endif
