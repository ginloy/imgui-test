#include "pico.hpp"

#include "libps2000/ps2000.h"
#include <cstdio>

#define TRUE 1
#define FALSE 0

std::vector<double> *channelAData = nullptr;
std::vector<double> *channelBData = nullptr;
std::mutex *channelALock = nullptr;
std::mutex *channelBLock = nullptr;
enPS2000Range voltageRangeGlob = DEFAULT_VOLTAGE_RANGE;

void streamCallback(int16_t **overviewBuffers, int16_t overflow,
                    uint32_t triggeredAt, int16_t triggered, int16_t auto_stop,
                    uint32_t nValues) {
  if (nValues <= 0) {
    return;
  }

  double maxVoltage;
  switch (voltageRangeGlob) {
  case PS2000_50MV:
    maxVoltage = 50. / 1000.;
    break;
  case PS2000_100MV:
    maxVoltage = 100. / 1000.;
    break;
  case PS2000_200MV:
    maxVoltage = 200. / 1000.;
    break;
  case PS2000_500MV:
    maxVoltage = 500. / 1000.;
    break;
  case PS2000_1V:
    maxVoltage = 1.;
    break;
  case PS2000_2V:
    maxVoltage = 2.;
    break;
  case PS2000_5V:
    maxVoltage = 5.;
    break;
  case PS2000_10V:
    maxVoltage = 10.;
    break;
  case PS2000_20V:
    maxVoltage = 20.;
    break;
  default:
    return;
  }

  channelALock->lock();
  channelBLock->lock();

  for (auto i = 0; i < nValues; ++i) {
    auto chAMax =
        (double)overviewBuffers[0][i] / (double)PS2000_MAX_VALUE * maxVoltage;
    auto chAMin =
        (double)overviewBuffers[1][i] / (double)PS2000_MAX_VALUE * maxVoltage;
    auto chBMax =
        (double)overviewBuffers[2][i] / (double)PS2000_MAX_VALUE * maxVoltage;
    auto chBMin =
        (double)overviewBuffers[3][i] / (double)PS2000_MAX_VALUE * maxVoltage;
    if (chAMax != chAMin || chBMax != chBMin) {
      printf("test\n");
    }
    channelAData->push_back(chAMax);
    channelBData->push_back(chBMax);
  }

  channelALock->unlock();
  channelBLock->unlock();
}

Scope::Scope() {}
Scope::~Scope() {
  if (streaming) {
    ps2000_stop(handle);
  }

  if (open) {
    ps2000_close_unit(handle);
  }

  channelAData = nullptr;
  channelBData = nullptr;
  channelALock = nullptr;
  channelBLock = nullptr;
  voltageRangeGlob = DEFAULT_VOLTAGE_RANGE;
}

void Scope::openScope() {
  auto res = ps2000_open_unit();
  if (res <= 0) {
    return;
  }
  handle = res;
  open = true;
}

bool Scope::isOpen() const { return open; }

void Scope::clearData() {
  channelA.dataLock.lock();
  channelB.dataLock.lock();

  channelA.data.clear();
  channelB.data.clear();

  channelA.dataLock.unlock();
  channelB.dataLock.unlock();
}

void Scope::setVoltageRange(enPS2000Range range) {
  auto prevRange = voltageRange;
  voltageRange = range;
  voltageRangeGlob = range;
  if (streaming && prevRange != range) {
    stopStream();
    startStream();
  }
}

void Scope::setStreamingMode(bool dc) {
  auto prev = this->dc;
  this->dc = dc;
  if (streaming && prev != dc) {
    stopStream();
    startStream();
  }
}

bool Scope::startStream() {
  if (streaming) {
    return true;
  }
  voltageRangeGlob = voltageRange;
  channelAData = &channelA.data;
  channelBData = &channelB.data;
  channelALock = &channelA.dataLock;
  channelBLock = &channelB.dataLock;

  ps2000_set_channel(handle, PS2000_CHANNEL_A, TRUE, dc, voltageRange);
  ps2000_set_channel(handle, PS2000_CHANNEL_B, TRUE, dc, voltageRange);
  ps2000_set_trigger(handle, PS2000_NONE, 0, PS2000_RISING, 0, 0);
  auto started = ps2000_run_streaming_ns(handle, SAMPLE_INTERVAL, TIME_UNITS,
                                         getSampleRate() * 10., FALSE, 1, 1e6);
  if (!started) {
    return false;
  }

  streaming = true;
  auto f = [](int16_t handle, std::atomic<bool> &streaming) {
    while (streaming) {
      ps2000_get_streaming_last_values(handle, streamCallback);
    }
  };

  streamTask = std::thread(f, handle, std::ref(streaming));

  return true;
}

void Scope::stopStream() {
  if (streaming) {
    streaming = false;
    streamTask.join();
    ps2000_stop(handle);
  }

  channelAData = nullptr;
  channelBData = nullptr;
  channelALock = nullptr;
  channelBLock = nullptr;
}

const Channel &Scope::getChannelA() const { return channelA; }
const Channel &Scope::getChannelB() const { return channelB; }

void Scope::lockChannels() {
  channelA.dataLock.lock();
  channelB.dataLock.lock();
}

void Scope::unlockChannels() {
  channelA.dataLock.unlock();
  channelB.dataLock.unlock();
}
