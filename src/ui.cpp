#include "ui.hpp"
#include "pico.hpp"
#include "processing.hpp"

#include <cmath>
#include <cstdlib>
#include <imgui.h>
#include <implot.h>
#include <iostream>
#include <iterator>
#include <libps2000/ps2000.h>
#include <range/v3/all.hpp>
#include <range/v3/range/conversion.hpp>
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
    auto temp = ImPlot::GetPlotLimits();
    if (abs(temp.X.Min - settings.limits.X.Min) > 1e-6 ||
        abs(temp.X.Max - settings.limits.X.Max) > 1e-6) {
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

    const auto range = settings.limits.X.Max - settings.limits.X.Min;
    auto scale = to_scale(settings.timebase);
    auto left = settings.limits.X.Min / scale / DELTA_TIME;
    auto right = settings.limits.X.Max / scale / DELTA_TIME;
    left = left < 0 ? 0. : left;
    left = left >= data.size() ? data.size() : left;
    right = right < 0 ? 0. : right;
    right = right >= data.size() ? data.size() : right;
    size_t size = right - left;
    auto stride = std::max(1UL, size / PLOT_SAMPLES);
    auto idxs =
        rv::iota((size_t)round(left)) | rv::take(size) | rv::stride(stride);
    auto xs =
        idxs |
        rv::transform([scale](auto e) { return e * DELTA_TIME * scale; }) |
        ranges::to_vector;
    auto ys = idxs | rv::transform([&data](auto e) { return data[e]; }) |
              ranges::to_vector;

    ImPlot::PlotLine(name.c_str(), xs.data(), ys.data(), xs.size());
  }
  ImPlot::EndPlot();
}

void drawScopeControls(ScopeSettings &settings, Scope &scope) {
  ImGui::BeginDisabled(settings.disableControls);

  if (ImGui::Checkbox("Run", &settings.run)) {
  }
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

  ImGui::SameLine();
  if (ImGui::Checkbox("Spectrum", &settings.showSpectrum)) {
    std::cout << "test" << std::endl;
    settings.resetScopeWindow = true;
  }

  ImGui::PushItemWidth(ImGui::CalcTextSize("2000 mV").x);
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

  ImGui::PopItemWidth();

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

void drawSpectrum(ScopeSettings &settings) {
  using namespace std::chrono_literals;
  static bool first = true;
  static auto [sendResult, recvResult] = mpsc::make<std::vector<double>>();
  static auto [sendData, recvData] =
      mpsc::make<std::pair<std::vector<double>, std::vector<double>>>();
  static std::vector<double> ys;
  static std::thread thread{[recv = std::move(recvData),
                             send = std::move(sendResult)]() mutable {
    while (true) {
      auto data = recv.flush();
      if (data.empty()) {
        std::this_thread::sleep_for(100ms);
        continue;
      }

      auto &&[dataA, dataB] = std::move(data.back());
      auto spectrumA = fft(std::move(dataA));
      auto spectrumB = fft(std::move(dataB));

      send.send(rv::zip(spectrumA, spectrumB) | rv::transform([](auto pair) {
                  auto &&[a, b] = pair;
                  auto temp = a / b;
                  auto res = 20 * log10(abs(temp));
                  return res;
                }) |
                ranges::to_vector);
    }
  }};
  if (first) {
    thread.detach();
    first = false;
  }

  if (settings.updateSpectrum) {
    std::cout << "new limits" << std::endl;
    auto limits = settings.limits.X;
    auto scale = to_scale(settings.timebase);
    ImPlotRange range{limits.Min / scale, limits.Max / scale};

    int left = std::round(range.Min / DELTA_TIME);
    int right = std::round(range.Max / DELTA_TIME);

    if (left < 0) {
      left = 0;
    }
    if (left >= settings.dataA.size()) {
      left = settings.dataA.size() - 1;
    }

    if (right < 0) {
      right = 0;
    }
    if (right >= settings.dataA.size()) {
      right = settings.dataA.size() - 1;
    }

    auto dataA = settings.dataA | rv::slice(left, right) | ranges::to_vector;
    auto dataB = settings.dataB | rv::slice(left, right) | ranges::to_vector;
    sendData.send(std::pair{std::move(dataA), std::move(dataB)});

    settings.updateSpectrum = false;
  }

  auto result = recvResult.flush();
  if (!result.empty()) {
    ys = std::move(result.back());
  }

  double bin_size = SAMPLE_RATE / 2 / ys.size();
  auto temp = rv::iota(0) | rv::take(ys.size()) |
              rv::transform([bin_size](auto i) { return i * bin_size; }) |
              rv::drop_while([&settings](auto i) {
                return i < settings.spectrumLimits.X.Min;
              }) |
              rv::take_while([&settings](auto &&i) {
                return i <= settings.spectrumLimits.X.Max;
              });

  std::cout << std::format("Distance: {}", sr::distance(temp));
  auto stride = std::max<size_t>(1, sr::distance(temp) / PLOT_SAMPLES);
  std::cout << std::format("Stride: {}", stride) << std::endl;
  auto xs = temp | rv::stride(stride) | ranges::to_vector;
  auto strided_ys = ys | rv::stride(stride) | ranges::to_vector;
  if (ImPlot::BeginPlot("Spectrum", ImGui::GetContentRegionAvail(), 0)) {
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
    auto available = ImGui::GetContentRegionAvail();
    auto flags = ImGuiChildFlags_None;
    if (settings.showSpectrum) {
      flags = ImGuiChildFlags_ResizeX;
      ImGui::SetNextWindowSizeConstraints({available.x * 0.2f, available.y},
                                          {available.x * 0.8f, available.y});
      available.x /= 2;
    }
    if (settings.resetScopeWindow) {
      flags = ImGuiChildFlags_None;
      settings.resetScopeWindow = false;
    }
    if (ImGui::BeginChild("ScopeHalf", available, flags)) {
      drawScope(settings, scope);
      ImGui::EndChild();
    }
    if (settings.showSpectrum) {
      ImGui::SameLine();
      if (ImGui::BeginChild("SpectrumHalf", ImGui::GetContentRegionAvail(),
                            0)) {
        drawSpectrum(settings);
        ImGui::EndChild();
      }
    }
    ImGui::EndChild();
  }
  if (ImGui::BeginChild("Controls", ImGui::GetContentRegionAvail(), 0)) {
    drawScopeControls(settings, scope);
    ImGui::EndChild();
  }
}

void ScopeSettings::fillRandomData(size_t samples) {
  auto iota = sv::iota(0) |
              sv::transform([](auto e) { return (double)e * DELTA_TIME; });
  auto dataA = iota | sv::transform(
                          [](auto e) { return 2. * sin(2 * M_PI * 1000 * e); });
  auto dataB =
      iota | sv::transform([](auto e) { return sin(2 * M_PI * 500 * e); });

  auto randomData =
      iota | sv::transform([](auto e) { return (double)rand() / RAND_MAX; });

  auto a = randomData | sv::take(samples) | ranges::to_vector;
  auto b =
      randomData | sv::drop(samples) | sv::take(samples) | ranges::to_vector;

  this->dataA.insert(this->dataA.end(), a.begin(), a.end());
  this->dataB.insert(this->dataB.end(), b.begin(), b.end());

  updateSpectrum = true;
}
