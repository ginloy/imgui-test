#include "ui.hpp"
#include "pico.hpp"
#include "processing.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <format>
#include <imgui.h>
#include <implot.h>
#include <iostream>
#include <libps2000/ps2000.h>
#include <numbers>
#include <range/v3/all.hpp>
#include <ranges>
#include <thread>

namespace sr = std::ranges;
namespace sv = std::views;
namespace rv = ranges::views;

namespace {
constexpr size_t PLOT_SAMPLES = 10000;
constexpr std::array SUPPORTED_RANGES = {
    PS2000_1V,   PS2000_2V,    PS2000_5V,    PS2000_10V,  PS2000_20V,
    PS2000_50MV, PS2000_100MV, PS2000_200MV, PS2000_500MV};
constexpr std::array SUPPORTED_TIMEBASES = {TimeBase::S, TimeBase::MS,
                                            TimeBase::US};
constexpr std::array SUPPORTED_SIGNALS = {SigGen::FreqSweep, SigGen::Noise};

std::string to_string(TimeBase tb);
std::string to_string(enPS2000Range range);
std::string to_string(SigGen signal);
double to_scale(TimeBase tb);
double to_scale(enPS2000Range range);
ImVec2 to_limits(enPS2000Range range);
void drawSplitter(bool vertical, float thickness, float *size0, float *size1,
                  float minSize0, float minSize1);
void drawSweepSettings(FreqSweepSettings &settings);
void drawSigGenControls(ScopeSettings &settings, Scope &scope);
void drawSpectrumControls(ScopeSettings &settings);
void drawScopeControls(ScopeSettings &settings, Scope &scope);
void drawControls(ScopeSettings &settings, Scope &scope);

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

std::string to_string(SigGen signal) {
  switch (signal) {
  case SigGen::Noise:
    return "Noise";
  case SigGen::FreqSweep:
    return "Frequency Sweep";
  };
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
void drawSplitter(bool vertical, float thickness, float *size0, float *size1,
                  float minSize0, float minSize1) {
  ImVec2 backup_pos = ImGui::GetCursorPos();
  if (vertical)
    ImGui::SetCursorPosY(backup_pos.y + *size0);
  else
    ImGui::SetCursorPosX(backup_pos.x + *size0);

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(
      ImGuiCol_ButtonActive,
      ImVec4(0, 0, 0,
             0)); // We don't draw while active/pressed because as we move the
                  // panes the splitter button will be 1 frame late
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.6f, 0.6f, 0.6f, 0.10f));
  ImGui::Button("##Splitter", ImVec2(!vertical ? thickness : -1.0f,
                                     vertical ? thickness : -1.0f));
  ImGui::PopStyleColor(3);

  ImGui::SetItemAllowOverlap(); // This is to allow having other buttons OVER
                                // our splitter.

  if (ImGui::IsItemActive()) {
    float mouse_delta =
        vertical ? ImGui::GetIO().MouseDelta.y : ImGui::GetIO().MouseDelta.x;

    // Minimum pane size
    if (mouse_delta < minSize0 - *size0)
      mouse_delta = minSize0 - *size0;
    if (mouse_delta > *size1 - minSize1)
      mouse_delta = *size1 - minSize1;

    // Apply resize
    *size0 += mouse_delta;
    *size1 -= mouse_delta;
  }
  ImGui::SetCursorPos(backup_pos);
}

void drawScopeControls(ScopeSettings &settings, Scope &scope) {
  ImGui::BeginGroup();
  auto toggled = ImGui::Checkbox("Run", &settings.run);

  if (toggled && settings.run) {
    settings.recv = scope.startStream();
    if (!settings.recv.has_value())
      settings.run = false;
  }

  if (toggled && !settings.run) {
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

  ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.2);
  if (ImGui::BeginCombo("Voltage Range",
                        to_string(settings.voltageRange).c_str())) {
    sr::for_each(
        rv::enumerate(SUPPORTED_RANGES), [&settings, &scope](auto pair) {
          auto [i, v] = pair;
          const bool selected = settings.voltageRange == v;
          if (ImGui::Selectable(to_string(v).c_str(), selected)) {
            if (settings.voltageRange != v) {
              settings.voltageRange = v;
              scope.setVoltageRange(v);
              auto new_limits = to_limits(v);
              ImPlot::SetNextAxisLimits(ImAxis_Y1, new_limits.x, new_limits.y,
                                        ImGuiCond_Always);
            }
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

  ImGui::PopItemWidth();

  if (ImGui::Button("Clear")) {
    settings.clearData();
    auto range = settings.limits.X.Max - settings.limits.X.Min;
    ImPlot::SetNextAxisLimits(ImAxis_X1, 0, 0 + range, ImGuiCond_Always);
  }
  ImGui::EndGroup();
}

void drawSigGenControls(ScopeSettings &settings, Scope &scope) {
  ImGui::BeginGroup();
  ImGui::BeginDisabled(settings.generate);
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.35);
  if (ImGui::BeginCombo("Signal",
                        to_string(settings.selectedSigType).c_str())) {
    static size_t selectedIdx =
        ranges::find(SUPPORTED_SIGNALS, settings.selectedSigType) -
        SUPPORTED_SIGNALS.begin();
    ranges::for_each(ranges::views::enumerate(SUPPORTED_SIGNALS),
                     [&settings](auto &&p) {
                       auto &&[i, e] = p;
                       auto selected = i == selectedIdx;
                       if (ImGui::Selectable(to_string(e).c_str(), selected)) {
                         selectedIdx = i;
                         if (settings.selectedSigType != e) {
                           settings.selectedSigType = e;
                         }
                       }
                       if (selected) {
                         ImGui::SetItemDefaultFocus();
                       }
                     });
    ImGui::EndCombo();
  }
  ImGui::EndDisabled();

  ImGui::SameLine();

  settings.generate = scope.isGenerating();
  auto toggled = ImGui::Checkbox("Generate", &settings.generate);
  if (toggled && settings.generate) {
    switch (settings.selectedSigType) {
    case (SigGen::FreqSweep):
      drawSweepSettings(settings.freqSweepSettings);
      if (settings.generate && !scope.isGenerating()) {
        scope.startFreqSweep(settings.freqSweepSettings.startFreq,
                             settings.freqSweepSettings.endFreq, 2., 0,
                             settings.freqSweepSettings.sweepDuration,
                             PS2000_UPDOWN);
      }
      break;
    case (SigGen::Noise):
      if (settings.generate && !scope.isGenerating() && !scope.startNoise(2.)) {
        settings.generate = false;
      }
      break;
    }
  }
  if (toggled && !settings.generate) {
    scope.stopSigGen();
  }
  if (settings.selectedSigType == SigGen::FreqSweep) {
    drawSweepSettings(settings.freqSweepSettings);
  }
  ImGui::EndGroup();
}

void drawSweepSettings(FreqSweepSettings &settings) {
  auto avail = ImGui::GetContentRegionAvail();
  auto flags = ImGuiInputTextFlags_CharsNoBlank |
               ImGuiInputTextFlags_CharsDecimal |
               ImGuiInputTextFlags_ParseEmptyRefVal;

  ImGui::PushItemWidth(0.2 * avail.x);

  ImGui::InputDouble("Start", &settings.startFreq, 50., 500., "%.2f", flags);
  ImGui::SameLine();
  ImGui::InputDouble("End", &settings.endFreq, 50., 500., "%.2f", flags);
  ImGui::SameLine();
  ImGui::InputDouble("Duration", &settings.sweepDuration, 5., 20., "%.2f",
                     flags);
  ImGui::PopItemWidth();

  settings.startFreq = std::max(settings.startFreq, 20.);
  settings.endFreq = std::min(settings.endFreq, 20000.);
  settings.sweepDuration = std::max(settings.sweepDuration, 1.);
  settings.sweepDuration = std::min(settings.sweepDuration, 30.);
}

void drawSpectrumControls(ScopeSettings &settings) {
  ImGui::BeginGroup();
  auto windowSize_str = std::format("{}", settings.windowSize);
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.2);
  if (ImGui::BeginCombo("Window Size", windowSize_str.c_str())) {
    static const auto powers =
        ranges::views::iota(5) | ranges::views::take(15) | ranges::to_vector;
    static size_t selected_idx =
        ranges::distance(powers | rv::take_while([&settings](auto &&e) {
                           return std::pow(2, e) != settings.windowSize;
                         }));

    ranges::for_each(rv::enumerate(powers), [&settings](auto &&p) {
      auto &&[i, power] = std::forward<decltype(p)>(p);
      const bool selected = i == selected_idx;
      const size_t windowSize = std::pow(2, power);
      if (ImGui::Selectable(std::format("{}", windowSize).c_str(), selected)) {
        std::cout << "Selected " << windowSize << std::endl;
        selected_idx = i;
        if (windowSize != settings.windowSize) {
          settings.updateSpectrum = true;
          settings.windowSize = windowSize;
        }
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    });
    ImGui::EndCombo();
  }
  auto prevSize = ImGui::GetItemRectSize();

  ImGui::SameLine();
  if (ImGui::Checkbox("Spectrum", &settings.showSpectrum)) {
    std::cout << "test" << std::endl;
    settings.resetScopeWindow = true;
  }

  ImGui::SetNextItemWidth(prevSize.x);
  if (ImGui::BeginCombo("Window Function", settings.windowFn.c_str())) {
    ranges::for_each(rv::enumerate(WINDOW_MAP), [&](auto &&p) {
      auto &&[i, e] = p;
      auto &&[s, f] = e;
      bool selected = s == settings.windowFn;
      if (ImGui::Selectable(s.c_str(), selected)) {
        if (settings.windowFn != s) {
          settings.updateSpectrum = true;
          settings.windowFn = s;
        }
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    });

    ImGui::EndCombo();
  }

  ImGui::EndGroup();
}

void drawControls(ScopeSettings &settings, Scope &scope) {
  if (ImGui::BeginTable("Full Controls", 2,
                        ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_Resizable,
                        {ImGui::GetContentRegionAvail().x, 0.})) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    drawScopeControls(settings, scope);
    ImGui::TableSetColumnIndex(1);
    drawSigGenControls(settings, scope);
    ImGui::EndTable();
  }

  ImGui::SeparatorText("Spectrum Controls");
  drawSpectrumControls(settings);
}

} // namespace

void drawScope(ScopeSettings &settings, Scope &scope) {
  static uint32_t frame = 0;
  ++frame;

  if (!ImPlot::BeginPlot("##Oscilloscope", ImGui::GetContentRegionAvail())) {
    return;
  }

  if (settings.recv.has_value()) {
    sr::for_each(settings.recv->flush_no_block(), [&settings](const auto &e) {
      settings.dataA.insert(settings.dataA.end(), e.dataA.begin(),
                            e.dataA.end());
      settings.dataB.insert(settings.dataB.end(), e.dataB.begin(),
                            e.dataB.end());
      settings.updateSpectrum = true;
    });
  }

  ImPlot::SetupAxes(to_string(settings.timebase).c_str(),
                    to_string(settings.voltageRange).c_str());
  auto vLimits = to_limits(settings.voltageRange);
  ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, vLimits.x, vLimits.y);
  ImPlot::SetupAxisLimits(ImAxis_Y1, vLimits.x, vLimits.y);
  ImPlot::SetupAxisLimits(ImAxis_X1, settings.limits.X.Min,
                          settings.limits.X.Max);

  if (settings.follow && scope.isStreaming() && frame % 5 == 0) {
    double latest = DELTA_TIME *
                    std::max(settings.dataA.size(), settings.dataB.size()) *
                    to_scale(settings.timebase);
    if (latest > settings.limits.X.Max || latest < settings.limits.X.Min) {
      auto range = settings.limits.X.Max - settings.limits.X.Min;
      ImPlot::SetupAxisLimits(ImAxis_X1, latest - range, latest,
                              ImPlotCond_Always);
    }
  } else {
    auto temp = ImPlot::GetPlotLimits();
    if (std::abs(temp.X.Min - settings.limits.X.Min) > 1e-6 ||
        std::abs(temp.X.Max - settings.limits.X.Max) > 1e-6) {
      settings.updateSpectrum = true;
    }
    settings.limits = temp;
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

    auto scale = to_scale(settings.timebase);
    auto left = settings.limits.X.Min / scale / DELTA_TIME;
    auto right = settings.limits.X.Max / scale / DELTA_TIME;
    left = left < 0 ? 0. : left;
    left = left >= data.size() ? data.size() : left;
    right = right < 0 ? 0. : right;
    right = right >= data.size() ? data.size() : right;
    size_t size = right - left;
    auto stride = std::max(static_cast<size_t>(1), size / PLOT_SAMPLES);
    auto idxs =
        rv::iota((size_t)round(left)) | rv::take(size) | rv::stride(stride);
    auto xs =
        idxs |
        rv::transform([scale](auto e) { return e * DELTA_TIME * scale; }) |
        ranges::to_vector;
    auto ys = idxs | rv::transform([&data, &settings](auto e) {
                return data[e] * to_scale(settings.voltageRange);
              }) |
              ranges::to_vector;

    ImPlot::PlotLine(name.c_str(), xs.data(), ys.data(), xs.size());
  }
  ImPlot::EndPlot();
}

void ScopeSettings::clearData() {
  dataA.clear();
  dataB.clear();

  updateSpectrum = true;
}

void drawSpectrum(ScopeSettings &settings) {
  using namespace std::chrono_literals;
  static bool first = true;
  static auto [sendResult, recvResult] = mpsc::make<std::vector<double>>();
  static auto [sendData, recvData] =
      mpsc::make<std::tuple<std::vector<double>, std::vector<double>, size_t,
                            WindowFunction>>();
  static std::vector<double> ys;
  static std::thread thread{
      [recv = std::move(recvData), send = std::move(sendResult)]() mutable {
        while (true) {
          auto data = recv.flush();
          if (data.empty()) {
            continue;
          }

          auto &&[dataA, dataB, windowSize, windowFn] = std::move(data.back());
          auto &&res =
              welch(std::move(dataA), std::move(dataB), windowSize, windowFn);

          send.send(std::move(res));
        }
      }};
  if (first) {
    thread.detach();
    first = false;
  }

  if (settings.updateSpectrum) {
    auto limits = settings.limits.X;
    auto scale = to_scale(settings.timebase);
    ImPlotRange range{limits.Min / scale, limits.Max / scale};

    int left = std::round(range.Min / DELTA_TIME);
    int right = std::round(range.Max / DELTA_TIME);

    if (left < 0) {
      left = 0;
    }
    if (left >= settings.dataA.size()) {
      left = settings.dataA.size();
    }

    if (right < 0) {
      right = 0;
    }
    if (right >= settings.dataA.size()) {
      right = settings.dataA.size();
    }

    auto dataA = settings.dataA | rv::slice(left, right) | ranges::to_vector;
    auto dataB = settings.dataB | rv::slice(left, right) | ranges::to_vector;
    sendData.send(std::tuple{std::move(dataA), std::move(dataB),
                             settings.windowSize,
                             WINDOW_MAP.at(settings.windowFn)});

    settings.updateSpectrum = false;
  }

  auto result = recvResult.flush_no_block();
  if (!result.empty()) {
    ys = std::move(result.back());
  }

  double bin_size = SAMPLE_RATE / 2 / ys.size();
  auto temp =
      rv::iota(0) | rv::take(ys.size()) |
      rv::transform([bin_size](auto i) { return std::pair{i, i * bin_size}; }) |
      rv::drop_while([&settings](auto i) {
        return i.second < settings.spectrumLimits.X.Min;
      }) |
      rv::take_while([&settings](auto &&i) {
        return i.second <= settings.spectrumLimits.X.Max;
      });

  auto stride = std::max<size_t>(1, sr::distance(temp) / PLOT_SAMPLES);
  auto xs = temp | rv::stride(stride) |
            rv::transform([](auto p) { return p.second; }) | ranges::to_vector;
  auto strided_ys = temp | rv::stride(stride) |
                    rv::transform([](auto p) { return ys[p.first]; }) |
                    ranges::to_vector;
  if (ImPlot::BeginPlot("Spectrum", ImGui::GetContentRegionAvail(),
                        ImPlotFlags_NoLegend)) {
    ImPlot::SetupAxes("Frequency", "Db", 0, 0);
    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0, 20000);
    ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, -100, 100);
    ImPlot::SetupAxesLimits(0., 20e3, -100., 100., ImPlotCond_Once);

    ImPlot::PlotLine("SpectrumPlot", xs.data(), strided_ys.data(), xs.size());

    settings.spectrumLimits = ImPlot::GetPlotLimits();
    ImPlot::EndPlot();
  }
}

void drawScopeTab(ScopeSettings &settings, Scope &scope) {
  auto size = ImGui::GetContentRegionAvail();
  if (ImGui::BeginChild("Scope", {size.x, size.y * 0.75f},
                        ImGuiChildFlags_ResizeY | ImGuiChildFlags_Borders)) {
    // auto available = ImGui::GetContentRegionAvail();
    // auto flags = ImGuiChildFlags_None;
    // if (settings.showSpectrum) {
    //   flags = ImGuiChildFlags_ResizeX;
    //   ImGui::SetNextWindowSizeConstraints({available.x * 0.2f, available.y},
    //                                       {available.x * 0.8f, available.y});
    //   available.x /= 2;
    // }
    // if (settings.resetScopeWindow) {
    //   flags = ImGuiChildFlags_None;
    //   settings.resetScopeWindow = false;
    // }
    // if (ImGui::BeginChild("ScopeHalf", available, flags)) {
    //   drawScope(settings, scope);
    //   ImGui::EndChild();
    // }
    // if (settings.showSpectrum) {
    //   ImGui::SameLine();
    //   if (ImGui::BeginChild("SpectrumHalf", ImGui::GetContentRegionAvail(),
    //                         0)) {
    //     drawSpectrum(settings);
    //     ImGui::EndChild();
    //   }
    // }
    // ImGui::EndChild();
    auto available = ImGui::GetContentRegionAvail();
    static auto scopeWidth =
        settings.showSpectrum ? available.x * 0.5f : available.x;
    if (settings.resetScopeWindow) {
      scopeWidth = settings.showSpectrum ? available.x * 0.5f : available.x;
      settings.resetScopeWindow = false;
    }
    float specWidth = available.x - scopeWidth;
    if (settings.showSpectrum) {
      drawSplitter(false, 20.f, &scopeWidth, &specWidth, 10., 10.);
    }
    if (ImGui::BeginChild("ScopeWindow", {scopeWidth, available.y})) {
      drawScope(settings, scope);
      ImGui::EndChild();
    }
    if (settings.showSpectrum) {
      ImGui::SameLine();
      if (ImGui::BeginChild("Spectrum", ImGui::GetContentRegionAvail())) {
        drawSpectrum(settings);
        ImGui::EndChild();
      }
    }
    ImGui::EndChild();
  }
  if (ImGui::BeginChild("Controls", ImGui::GetContentRegionAvail(), 0)) {
    drawControls(settings, scope);
    ImGui::EndChild();
  }
}

void ScopeSettings::fillRandomData(size_t samples) {
  auto iota = sv::iota(0) |
              sv::transform([](auto e) { return (double)e * DELTA_TIME; });
  auto dataA = iota | sv::transform([](auto e) {
                 return 2. * sin(2 * std::numbers::pi * 1000 * e);
               });
  auto dataB = iota | sv::transform([](auto e) {
                 return sin(2 * std::numbers::pi * 500 * e);
               });

  auto randomData =
      iota | sv::transform([](auto e) { return (double)rand() / RAND_MAX; });

  auto a = dataA | sv::take(samples) | ranges::to_vector;
  auto b = dataB | sv::take(samples) | ranges::to_vector;

  this->dataA.insert(this->dataA.end(), a.begin(), a.end());
  this->dataB.insert(this->dataB.end(), b.begin(), b.end());

  updateSpectrum = true;
}
