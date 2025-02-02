#ifndef PICO_HPP
#define PICO_HPP

#include <mutex>
#include <vector>

struct StreamBuffers {
  std::vector<double> bufferA;
  std::vector<double> bufferB;
};

extern StreamBuffers streamBuffers;
extern std::mutex streamBuffersLock;

void streamCallback();

void startStream();

#endif
