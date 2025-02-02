#include "pico.hpp"

StreamBuffers streamBuffers{};
std::mutex streamBuffersLock{};

void streamCallback() { streamBuffersLock.lock(); }

void startStream() {}
