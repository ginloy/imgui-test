#ifndef UI_HPP
#define UI_HPP

#include "mpsc.hpp"
#include "pico.hpp"
#include "processing.hpp"

#include <implot.h>
#include <libps2000/ps2000.h>
#include <optional>
#include <vector>

enum class TimeBase { US, MS, S };
enum class SigGen { Noise, FreqSweep };

inline constexpr TimeBase DEFAULT_TIMEBASE = TimeBase::S;

struct FreqSweepSettings {
  double startFreq = 1.;
  double endFreq = 1000.;
  double sweepDuration = 5.;
};

struct ScopeSettings {
  enPS2000Range voltageRange = DEFAULT_VOLTAGE_RANGE;
  TimeBase timebase = TimeBase::S;
  ImPlotRect limits = {0, 10, -10, 10};
  ImPlotRect spectrumLimits = {0, 20e3, -100, 100};
  size_t windowSize = 1 << 16;
  std::string windowFn = WINDOW_MAP.begin()->first;

  SigGen selectedSigType = SigGen::Noise;
  FreqSweepSettings freqSweepSettings;

  bool disableControls = false;
  bool run = false;
  bool follow = false;
  bool generate = false;
  bool showSpectrum = false;
  bool resetScopeWindow = false;
  bool updateSpectrum = false;

  std::optional<mpsc::Recv<StreamResult>> recv;
  std::vector<double> dataA;
  std::vector<double> dataB;

  void clearData();
  void fillRandomData(size_t samples);
};

void drawScope(ScopeSettings &settings, Scope &scope);
void drawScopeTab(ScopeSettings &settings, Scope &scope);

#endif
