#include "ui.hpp"
#include "pico.hpp"

#include <cmath>
#include <imgui.h>
#include <implot.h>
#include <iostream>
#include <iterator>
#include <libps2000/ps2000.h>
#include <range/v3/all.hpp>
#include <range/v3/range/conversion.hpp>
#include <ranges>

namespace sr = std::ranges;
namespace sv = std::views;
namespace rv = ranges::views;

namespace {
constexpr size_t PLOT_SAMPLES = 100000;
constexpr std::array SUPPORTED_RANGES = {
    PS2000_1V,   PS2000_2V,    PS2000_5V,    PS2000_10V,  PS2000_20V,
    PS2000_50MV, PS2000_100MV, PS2000_200MV, PS2000_500MV};
constexpr std::array SUPPORTED_TIMEBASES = {TimeBase::S, TimeBase::MS,
                                            TimeBase::US};

std::string to_string(TimeBase tb);
std::string to_string(enPS2000Range range);
double to_scale(TimeBase tb);
double to_scale(enPS2000Range range);
ImVec2 to_limits(enPS2000Range range);

std::string to_string(TimeBase tb) {
  switch (tb) {
  case TimeBase::US:
    return "us";
  case TimeBase::MS:
    return "ms";
  case TimeBase::S:
    return "s";
  }
}

std::string to_string(enPS2000Range range) {
  switch (range) {
  case PS2000_10MV:
    return "10mV";
  case PS2000_20MV:
    return "20mV";
  case PS2000_50MV:
    return "50mV";
  case PS2000_100MV:
    return "100mV";
  case PS2000_200MV:
    return "200mV";
  case PS2000_500MV:
    return "500mV";
  case PS2000_1V:
    return "1V";
  case PS2000_2V:
    return "2V";
  case PS2000_5V:
    return "5V";
  case PS2000_10V:
    return "10V";
  case PS2000_20V:
    return "20V";
  case PS2000_50V:
    return "50V";
  case PS2000_MAX_RANGES:
    return "";
  }
}

double to_scale(TimeBase tb) {
  switch (tb) {
  case TimeBase::US:
    return 1e6;
  case TimeBase::MS:
    return 1e3;
  case TimeBase::S:
    return 1.;
  }
}

double to_scale(enPS2000Range range) {
  switch (range) {
  case PS2000_20MV:
  case PS2000_50MV:
  case PS2000_100MV:
  case PS2000_200MV:
  case PS2000_500MV:
    return 1000;
  default:
    return 1.;
  }
}

ImVec2 to_limits(enPS2000Range range) {
  switch (range) {
  case PS2000_100MV:
    return {-100, 100};

  case PS2000_200MV:
    return {-200, 200};

  case PS2000_500MV:
    return {-500, 500};

  case PS2000_1V:
    return {-1, 1};

  case PS2000_2V:
    return {-2, 2};

  case PS2000_5V:
    return {-5, 5};

  case PS2000_10MV:
  case PS2000_10V:
    return {-10, 10};

  case PS2000_20MV:
  case PS2000_20V:
    return {-20, 20};

  case PS2000_50MV:
  case PS2000_50V:
    return {-50, 50};

  case PS2000_MAX_RANGES:
    return {0, 0};
  }
}

} // namespace

void drawScope(ScopeSettings &settings, Scope &scope) {
  static uint32_t frame = 0;
  ++frame;

  if (!ImPlot::BeginPlot("##Oscilloscope", ImGui::GetContentRegionAvail())) {
    return;
  }

  if (settings.recv.has_value()) {
    sr::for_each(settings.recv->flush(), [&settings](const auto &e) {
      settings.dataA.insert(settings.dataA.end(), e.dataA.begin(),
                            e.dataA.end());
      settings.dataB.insert(settings.dataB.end(), e.dataB.begin(),
                            e.dataB.end());
    });
  }

  ImPlot::SetupAxes(to_string(settings.timebase).c_str(),
                    to_string(settings.voltageRange).c_str());
  auto vLimits = to_limits(settings.voltageRange);
  ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, vLimits.x, vLimits.y);
  ImPlot::SetupAxisLimits(ImAxis_Y1, vLimits.x, vLimits.y);
  ImPlot::SetupAxisLimits(ImAxis_X1, settings.limits.X.Min,
                          settings.limits.X.Max);

  if (settings.follow && scope.isStreaming() && frame % 3 == 0) {
    double latest = DELTA_TIME *
                    std::max(settings.dataA.size(), settings.dataB.size()) *
                    to_scale(settings.timebase);
    if (latest > settings.limits.X.Max | latest < settings.limits.X.Min) {
      auto range = settings.limits.X.Max - settings.limits.X.Min;
      ImPlot::SetupAxisLimits(ImAxis_X1, latest - range, latest,
                              ImPlotCond_Always);
    }
  } else {
    settings.limits = ImPlot::GetPlotLimits();
  }

  for (int i = 0; i < 2; ++i) {
    std::string name;
    const std::vector<double> &data = ([i, &settings, &name]() {
      if (i == 0) {
        name = "Channel A";
        return settings.dataA;
      } else {
        name = "Channel B";
        return settings.dataB;
      }
    })();

    const auto range = settings.limits.X.Max - settings.limits.X.Min;
    auto times = sv::iota(0) | sv::take(PLOT_SAMPLES) |
                 sv::transform([range, &settings](auto i) {
                   return i / 1000. * range + settings.limits.X.Min;
                 });
    auto vals = times | sv::transform([&settings](auto t) {
                  int64_t idx =
                      std::round(t / to_scale(settings.timebase) / DELTA_TIME);
                  return std::pair{t, idx};
                }) |
                sv::filter([&data](auto pair) {
                  return pair.second >= 0 && pair.second < data.size();
                }) |
                sv::transform([&data, &settings](auto pair) {
                  auto &&[t, i] = pair;
                  double val = data[i] * to_scale(settings.voltageRange);
                  return std::pair{t, val};
                });
    auto xs = vals |
              sv::transform([](const auto &pair) { return pair.first; }) |
              ranges::to_vector;
    auto ys = vals |
              sv::transform([](const auto &pair) { return pair.second; }) |
              ranges::to_vector;

    ImPlot::PlotLine(name.c_str(), xs.data(), ys.data(), xs.size());
  }
  ImPlot::EndPlot();
}

void drawScopeControls(ScopeSettings &settings, Scope &scope) {
  ImGui::BeginDisabled(settings.disableControls);

  ImGui::Checkbox("Run", &settings.run);
  if (settings.run && !scope.isStreaming()) {
    std::cout << "start" << std::endl;
    auto temp = scope.startStream();
    settings.recv = scope.startStream();
  }
  if (!settings.run && scope.isStreaming()) {
    std::cout << "stop" << std::endl;
    scope.stopStream();
    settings.recv = std::nullopt;
  }

  if (scope.isStreaming()) {
    settings.run = true;
  } else {
    settings.run = false;
  }

  ImGui::SameLine();
  ImGui::Checkbox("Follow", &settings.follow);
  ImGui::SameLine();

  static bool gen = false;
  ImGui::Checkbox("Noise", &gen);
  if (gen && !scope.isGenerating()) {
    scope.startNoise(2.0);
    // scope.startFreqSweep(1, 20, 2.0, 0, 30, PS2000_UPDOWN);
  }
  if (!gen && scope.isGenerating()) {
    scope.stopSigGen();
  }

  if (ImGui::BeginCombo("Voltage Range",
                        to_string(settings.voltageRange).c_str())) {
    static size_t selected_idx =
        sr::distance(SUPPORTED_RANGES | sv::take_while([](const auto e) {
                       return e != DEFAULT_VOLTAGE_RANGE;
                     }));
    sr::for_each(rv::enumerate(SUPPORTED_RANGES),
                 [&settings, &scope](auto pair) {
                   auto [i, v] = pair;
                   const bool selected = i == selected_idx;
                   if (ImGui::Selectable(to_string(v).c_str(), selected)) {
                     selected_idx = i;
                     auto range = SUPPORTED_RANGES[i];
                     settings.voltageRange = range;
                     scope.setVoltageRange(range);
                     auto new_limits = to_limits(range);
                     ImPlot::SetNextAxisLimits(ImAxis_Y1, new_limits.x,
                                               new_limits.y, ImGuiCond_Always);
                   }
                   if (selected) {
                     ImGui::SetItemDefaultFocus();
                   }
                 });
    ImGui::EndCombo();
  }

  ImGui::SameLine();

  if (ImGui::BeginCombo("Time Base", to_string(settings.timebase).c_str())) {
    static size_t selected_idx =
        sr::distance(SUPPORTED_TIMEBASES | sv::take_while([](const auto e) {
                       return e != DEFAULT_TIMEBASE;
                     }));
    sr::for_each(rv::enumerate(SUPPORTED_TIMEBASES), [&settings](auto pair) {
      auto [i, v] = pair;
      const bool selected = i == selected_idx;
      if (ImGui::Selectable(to_string(v).c_str(), selected)) {
        selected_idx = i;
        settings.timebase = SUPPORTED_TIMEBASES[i];
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    });
    ImGui::EndCombo();
  }

  if (ImGui::Button("Clear")) {
    settings.clearData();
    auto range = settings.limits.X.Max - settings.limits.X.Min;
    ImPlot::SetNextAxisLimits(ImAxis_X1, 0, 0 + range, ImGuiCond_Always);
  }
  ImGui::EndDisabled();
}

void ScopeSettings::clearData() {
  dataA.clear();
  dataB.clear();
}
