#include "ui.hpp"
#include "pico.hpp"

#include <cmath>
#include <imgui.h>
#include <implot.h>
#include <libps2000/ps2000.h>
#include <ranges>

namespace {
constexpr size_t PLOT_SAMPLES = 100000;

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
  namespace sr = std::ranges;
  namespace sv = std::views;

  static uint32_t frame = 0;
  ++frame;

  if (!ImPlot::BeginPlot("##Oscilloscope", ImGui::GetContentRegionAvail())) {
    return;
  }

  if (settings.recv.has_value()) {
    auto res = settings.recv->flush();
    sr::for_each(res, [&settings](const auto &e) {
      settings.dataA.append_range(e.dataA);
      settings.dataB.append_range(e.dataB);
    });
  }

  ImPlot::SetupAxes(to_string(settings.timebase).c_str(),
                    to_string(settings.voltageRange).c_str());
  auto vLimits = to_limits(settings.voltageRange);
  ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, vLimits.x, vLimits.y);
  ImPlot::SetupAxisLimits(ImAxis_X1, settings.limits.X.Min,
                          settings.limits.X.Min);

  if (settings.follow && scope.isStreaming() && frame % 3 == 0) {
    double latest = DELTA_TIME *
                    std::max(settings.dataA.size(), settings.dataB.size()) *
                    to_scale(settings.timebase);
    if (latest > settings.limits.X.Max) {
      auto range = settings.limits.X.Max - settings.limits.X.Min;
      ImPlot::SetupAxisLimits(ImAxis_Y1, latest - range, latest,
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
    auto vals = times | sv::transform([range, &settings](auto t) {
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
              sr::to<std::vector<double>>();
    auto ys = vals |
              sv::transform([](const auto &pair) { return pair.second; }) |
              sr::to<std::vector<double>>();

    ImPlot::PlotLine(name.c_str(), xs.data(), ys.data(), xs.size());
    ImPlot::EndPlot();
  }
}
