#ifndef UI_HPP
#define UI_HPP

#include "mpsc.hpp"
#include "pico.hpp"

#include <implot.h>
#include <libps2000/ps2000.h>
#include <optional>
#include <vector>

enum class TimeBase { US, MS, S };

inline constexpr TimeBase DEFAULT_TIMEBASE = TimeBase::S;

struct ScopeSettings {
  enPS2000Range voltageRange = DEFAULT_VOLTAGE_RANGE;
  TimeBase timebase = TimeBase::S;
  ImPlotRect limits = {0, 10, -10, 10};
  ImPlotRect spectrumLimits = {0, 20e3, -100, 100};
  bool disableControls = false;
  bool run = false;
  bool follow = false;
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
void drawScopeControls(ScopeSettings &settings, Scope &scope);
void drawScopeTab(ScopeSettings &settings, Scope &scope);

#endif
