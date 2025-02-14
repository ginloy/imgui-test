cmake_minimum_required(VERSION 3.21)


if (NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 23)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_SHARED_LIBS off)

project(imgui-test LANGUAGES CXX C)

include(FetchContent)
FetchContent_Declare(
  FFTW3
  URL https://www.fftw.org/fftw-3.3.10.tar.gz
  URL_HASH MD5=8ccbf6a5ea78a16dbc3e1306e234cc5c
)
FetchContent_Declare(
  glfw3
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
FetchContent_Declare(
  range-v3
  GIT_REPOSITORY https://github.com/ericniebler/range-v3.git
  GIT_TAG 0.12.0
)
FetchContent_MakeAvailable(FFTW3 glfw3 range-v3)

add_subdirectory(extern)
add_subdirectory(mpsc)

find_package(OpenGL REQUIRED)
find_package(OpenMP REQUIRED)

# add_executable(${PROJECT_NAME} src/main.cpp src/ui.cpp src/pico.cpp src/processing.cpp)
# target_include_directories(${PROJECT_NAME} PRIVATE mpsc include ${fftw3_SOURCE_DIR}/api)

# if (APPLE)
#   target_link_directories(${PROJECT_NAME} PRIVATE /Library/Frameworks/PicoSDK.framework/Libraries/libps2000)
#   target_include_directories(${PROJECT_NAME} PRIVATE /Library/Frameworks/PicoSDK.framework/Headers)
# endif()

# target_link_libraries(${PROJECT_NAME} PRIVATE imgui imgui-backends implot range-v3 fftw3 ps2000 OpenGL::GL OpenMP::OpenMP_CXX)

add_subdirectory(src)

