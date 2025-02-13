
// Dear ImGui: standalone example application for GLFW + OpenGL 3, using
// programmable pipeline (GLFW is a cross-platform general purpose library for
// handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation,
// etc.) If you are new to Dear ImGui, read documentation from the docs/ folder
// + read the top of imgui.cpp. Read online:
// https://github.com/ocornut/imgui/tree/master/docs

#include <format>
#define _USE_MATH_DEFINES

#include "globals.hpp"
#include "pico.hpp"
#include "processing.hpp"
#include "ui.hpp"

#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <string>
#include <vector>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#undef GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

// #include "libps2000/ps2000.h"

static void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

struct ChannelTemp {
  ImVec4 color = ImVec4(1, 1, 1, 1);
  std::vector<double> data;
  std::vector<std::complex<double>> transformedData;

  void updateTransform() { transformedData = fft(data); }
};

struct OscilloscopeSettings {
  bool running = false;
  bool follow = false;
  float timeRange = 1.0;

  ChannelTemp channels[2];
};

struct TestData {
  bool windowOpen = false;
  bool running = false;
  double duration = 0.0;
  double startTime = 0.0;

  ChannelTemp channels[2];

  void clearData() {
    for (int i = 0; i < 2; ++i) {
      channels[i].data.clear();
      channels[i].transformedData.clear();
    }
  }

  void updateTransforms() {
    for (auto &ch : channels) {
      ch.updateTransform();
    }
  }

  std::pair<std::vector<double>, std::vector<double>> getSpectrum() {
    updateTransforms();

    const size_t points = std::min(channels[0].transformedData.size(),
                                   channels[1].transformedData.size());

    const double binSize = SAMPLE_RATE / 2 / points;
    std::vector<double> xs(points);
    std::vector<double> ys(points);
    for (size_t i = 0; i < points; ++i) {
      xs[i] = i * binSize;
      auto transfer =
          channels[0].transformedData[i] / channels[1].transformedData[i];
      ys[i] = std::abs(transfer);
    }

    return {xs, ys};
  }
};

void generateSinewave(std::vector<double> &buffer, float time, double amplitude,
                      double frequency, double verticalOffset = 0.0,
                      double horizontalOffset = 0.0) {
  size_t idx = buffer.size();

  for (size_t i = buffer.size(); i < (size_t)(time / DELTA_TIME); ++i) {
    buffer.push_back(amplitude * sin(2 * M_PI * frequency * i * DELTA_TIME));
  }
}
void generateCosinewave(std::vector<double> &buffer, float time,
                        double amplitude, double frequency,
                        double verticalOffset = 0.0,
                        double horizontalOffset = 0.0) {
  size_t idx = buffer.size();

  for (size_t i = buffer.size(); i < (size_t)(time / DELTA_TIME); ++i) {
    buffer.push_back(amplitude * cos(2 * M_PI * frequency * i * DELTA_TIME));
  }
}

void DrawOscilloscope(OscilloscopeSettings &settings, const ImVec2 &size) {
  static float time = 0;
  if (settings.running)
    time += ImGui::GetIO().DeltaTime;

  generateSinewave(settings.channels[0].data, time, 3.0, 5);
  generateCosinewave(settings.channels[1].data, time, 5.0, 2);

  if (ImPlot::BeginPlot("##Oscilloscope", size, 0)) {
    ImPlot::SetupAxes(nullptr, nullptr);
    ImPlot::SetupAxesLimits(0, 5, -10, 10);
  }
  if (settings.follow && settings.running) {
    float latest = DELTA_TIME * std::max(settings.channels[0].data.size(),
                                         settings.channels[1].data.size());
    ImPlot::SetupAxisLimits(ImAxis_X1, latest - settings.timeRange, latest,
                            ImPlotCond_Always);
  }

  auto limits = ImPlot::GetPlotLimits();
  auto range = limits.X.Max - limits.X.Min;
  // if (settings.follow && settings.running) {
  //   float latest = DELTA_TIME * std::max(settings.channels[0].data.size(),
  //                                        settings.channels[1].data.size());
  //   ImPlot::SetupAxisLimits(ImAxis_X1, latest - settings.timeRange, latest,
  //   ImPlotCond_Always);
  // }

  settings.timeRange = range;

  // ImGuiCond_Always);

  // ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));
  // ImPlot::PushStyleColor(ImPlotCol_PlotBg, IM_COL32(20, 20, 20, 255));
  // ImPlot::PushStyleColor(ImPlotCol_AxisBg, IM_COL32(0, 0, 0, 0));

  // Draw channels
  for (int i = 0; i < 2; ++i) {
    auto &ch = settings.channels[i];
    if (ch.data.empty())
      continue;

    // Scale data to volts/div and apply offset

    // Plot waveform
    ImPlot::SetNextLineStyle(ch.color);
    auto start = limits.X.Min - range / 2;
    auto end = limits.X.Max + range / 2;

    const auto name = "ChannelTemp " + std::to_string(i + 1);
    std::vector<double> xs;
    std::vector<double> ys;
    for (double i = start; i < end; i += (end - start) / PLOT_SAMPLES) {
      int idx = round(i / DELTA_TIME);
      if (idx < 0) {
        continue;
      }
      if (idx >= ch.data.size()) {
        break;
      }

      xs.push_back(idx * DELTA_TIME);
      ys.push_back(ch.data[idx]);
    }

    ImPlot::PlotLine(name.c_str(), xs.data(), ys.data(), xs.size());
  }

  ImPlot::EndPlot();

  if (range > 60.0) {
    range = 60.0;
    ImPlot::SetNextAxisLimits(ImAxis_X1, limits.X.Min, limits.X.Min + range,
                              ImGuiCond_Always);
  }
}

void ShowControls(OscilloscopeSettings &settings) {
  ImGui::Checkbox("Run", &settings.running);
  ImGui::SameLine();
  ImGui::Checkbox("Follow", &settings.follow);

  ImGui::BeginDisabled(!settings.follow || !settings.running);
  ImGui::DragFloat("Time Range", &settings.timeRange, 0.01, 0.0, 60.0,
                   "%.3f seconds", ImGuiSliderFlags_AlwaysClamp);
  ImGui::EndDisabled();

  // ChannelTemp controls
  for (int ch = 0; ch < 2; ch++) {
    ImGui::PushID(ch);
    ImGui::Separator();
    ImGui::SameLine();
    ImGui::ColorEdit3("Color", (float *)&settings.channels[ch].color,
                      ImGuiColorEditFlags_NoInputs);
    ImGui::PopID();
  }

  // ImGui::End();
}

int main(int, char **) {

  Scope &scope = Scope::getInstance();
  scope.openScope();

  // if (scope.isOpen()) {

  //   scope.setStreamingMode(true);
  //   scope.setVoltageRange(PS2000_5V);

  //   auto recv = scope.startStream().value();
  //   auto start = std::chrono::high_resolution_clock::now();
  //   scope.startFreqSweep(1, 20, 2, 0, 10, PS2000_UP);
  //   // scope.startNoise(2.0);

  //   size_t samples = 0;
  //   while (std::chrono::high_resolution_clock::now() - start <
  //          std::chrono::seconds(10)) {
  //     auto res = recv.flush();
  //     for (const auto &e : res) {
  //       samples += e.dataA.size();
  //       for (auto p : e.dataA) {
  //         printf("%.3f\n", p);
  //       }
  //     }
  //   }
  //   scope.stopStream();
  //   scope.stopSigGen();

  //   auto timeTaken = std::chrono::high_resolution_clock::now() - start;
  //   printf("%zu samples in %lld seconds\n", samples,
  //          std::chrono::duration_cast<std::chrono::seconds>(timeTaken).count());
  // }

  // Setup window
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

  // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
  // GL ES 2.0 + GLSL 100
  const char *glsl_version = "#version 100";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
  // GL 3.2 + GLSL 150
  const char *glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#else
  // GL 3.0 + GLSL 130
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+
  // only glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // 3.0+ only
#endif

  // Create window with graphics context
  GLFWwindow *window = glfwCreateWindow(
      1280, 720, "Dear ImGui GLFW+OpenGL3 example", NULL, NULL);
  if (window == NULL)
    return 1;
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enableo

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsClassic();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Load Fonts
  // - If no fonts are loaded, dear imgui will use the default font. You can
  // also load multiple fonts and use ImGui::PushFont()/PopFont() to select
  // them.
  // - AddFontFromFileTTF() will return the ImFont* so you can store it if you
  // need to select the font among multiple.
  // - If the file cannot be loaded, the function will return NULL. Please
  // handle those errors in your application (e.g. use an assertion, or
  // display an error and quit).
  // - The fonts will be rasterized at a given size (w/ oversampling) and
  // stored into a texture when calling
  // ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame
  // below will call.
  // - Read 'docs/FONTS.md' for more instructions and details.
  // - Remember that in C/C++ if you want to include a backslash \ in a string
  // literal you need to write a double backslash \\ !
  // io.Fonts->AddFontDefault();
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
  // ImFont* font =
  // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f,
  // NULL, io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != NULL);

  // Our state
  bool show_demo_window = true;
  bool show_another_window = false;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // OscilloscopeSettings settings;
  // settings.channels[0].color = ImVec4(1, 0.5f, 0, 1);
  // settings.channels[1].color = ImVec4(0, 1, 0.5f, 1);

  TestData testData;
  ScopeSettings settings;
  settings.fillRandomData(SAMPLE_RATE * 10);

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to
    // tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to
    // your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input
    // data to your main application. Generally you may always pass all inputs
    // to dear imgui, and hide them from your application based on those two
    // flags.
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in
    // ImGui::ShowDemoWindow()! You can browse its code to learn more about
    // Dear ImGui!).
    // if (show_demo_window)
    //   ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End
    // pair to created a named window.
    {
      static float f = 0.0f;
      static int counter = 0;

      ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!"
                                     // and append into it.

      ImGui::Text("This is some useful text."); // Display some text (you can
                                                // use a format strings too)
      ImGui::Checkbox("Demo Window",
                      &show_demo_window); // Edit bools storing our window
                                          // open/close state
      ImGui::Checkbox("Another Window", &show_another_window);

      ImGui::SliderFloat("float", &f, 0.0f,
                         1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
      ImGui::ColorEdit3(
          "clear color",
          (float *)&clear_color); // Edit 3 floats representing a color

      if (ImGui::Button("Button")) // Buttons return true when clicked (most
                                   // widgets return true when edited/activated)
        counter++;
      ImGui::SameLine();
      ImGui::Text("counter = %d", counter);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                  1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      ImGui::End();
    }

    // 3. Show another simple window.
    if (show_another_window) {
      ImGui::Begin(
          "Another Window",
          &show_another_window); // Pass a pointer to our bool variable (the
                                 // window will have a closing button that
                                 // will clear the bool when clicked)
      ImGui::Text("Hello from another window!");
      if (ImGui::Button("Close Me"))
        show_another_window = false;
      ImGui::End();
    }

    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::Begin("Scope", NULL,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (ImGui::BeginTabBar("MainTabs")) {
      if (ImGui::BeginTabItem("Scope", NULL)) {
        // DrawOscilloscope(settings,
        //                  ImVec2(ImGui::GetContentRegionAvail().x,
        //                         ImGui::GetContentRegionAvail().y * 0.6));
        // static ScopeSettings settings;
        // drawScope(settings, scope);
        // drawScopeControls(settings, scope);
        drawScopeTab(settings, scope);
        // ShowControls(settings);
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }
    ImGui::End();

    auto text = std::format("{:.2f} FPS", ImGui::GetIO().Framerate);
    auto size = ImGui::CalcTextSize(text.c_str());
    auto windowPadding = ImGui::GetStyle().WindowPadding;
    auto framePadding = ImGui::GetStyle().FramePadding;
    ImGui::SetNextWindowSize({size.x + (framePadding.x + windowPadding.x) * 2,
                              size.y + (framePadding.y + windowPadding.y) * 2});
    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x - size.x -
                                 framePadding.x - windowPadding.x - 10.f,
                             0.f + 5.f});
    ImGui::SetNextWindowBgAlpha(0.75);
    ImGui::Begin("FPS Overlay", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoInputs);
    ImGui::TextUnformatted(text.c_str());
    ImGui::End();

    // ImPlot::ShowDemoWindow(nullptr);

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  ImPlot::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
