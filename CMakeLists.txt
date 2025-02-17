cmake_minimum_required(VERSION 3.21)


if (NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 23)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_SHARED_LIBS off)

project(imgui-test LANGUAGES CXX C)

include(CTest)
include(FetchContent)
FetchContent_Declare(
  FFTW3
  URL https://www.fftw.org/fftw-3.3.10.tar.gz
  URL_HASH MD5=8ccbf6a5ea78a16dbc3e1306e234cc5c
  FIND_PACKAGE_ARGS NAMES FFTW3
)
FetchContent_Declare(
  glfw3
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
  FIND_PACKAGE_ARGS NAMES glfw3
)
FetchContent_Declare(
  range-v3
  GIT_REPOSITORY https://github.com/ericniebler/range-v3.git
  GIT_TAG 0.12.0
  FIND_PACKAGE_ARGS NAMES range-v3
)
FetchContent_Declare(
  gTest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.16.0
  FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(FFTW3 glfw3 range-v3 gTest)

add_subdirectory(extern)
add_subdirectory(mpsc)

find_package(OpenGL REQUIRED)
find_package(OpenMP REQUIRED)

add_executable(${PROJECT_NAME})
add_subdirectory(src)
add_subdirectory(include)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw ps2000 OpenMP::OpenMP_CXX OpenGL::GL imgui implot imgui-backends range-v3::range-v3 mpsc fftw3)

if (DEFINED FFTW3_FOUND)
  target_include_directories(${PROJECT_NAME} PRIVATE ${FFTW3_INCLUDE_DIRS})
  target_link_directories(${PROJECT_NAME} PRIVATE ${FFTW3_LIBRARY_DIRS})
endif()

if (APPLE)
  target_link_directories(${PROJECT_NAME} PRIVATE /Library/Frameworks/PicoSDK.framework/Libraries/libps2000)
  target_include_directories(${PROJECT_NAME} PRIVATE /Library/Frameworks/PicoSDK.framework/Headers)
endif()


