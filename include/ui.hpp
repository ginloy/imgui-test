#ifndef UI_HPP
#define UI_HPP

#include "pico.hpp"
#include "mpsc.hpp"

#include <libps2000/ps2000.h>
#include <optional>
#include <vector>
#include <implot.h>

enum class TimeBase { US, MS, S };

inline constexpr TimeBase DEFAULT_TIMEBASE = TimeBase::S;

struct ScopeSettings {
  enPS2000Range voltageRange = DEFAULT_VOLTAGE_RANGE;
  TimeBase timebase = TimeBase::S;
  ImPlotRect limits = {0, 10, -10, 10};
  bool disableControls = false;
  bool run = false;
  bool follow = false;

  std::optional<mpsc::Recv<StreamResult>> recv;
  std::vector<double> dataA;
  std::vector<double> dataB;

  void clearData();
};

void drawScope(ScopeSettings &settings, Scope &scope);
void drawScopeControls(ScopeSettings &settings, Scope &scope);

#endif
