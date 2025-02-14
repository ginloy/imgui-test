
add_executable(${PROJECT_NAME} main.cpp processing.cpp ui.cpp pico.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include ${fftw3_SOURCE_DIR}/api)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw fftw3 ps2000 OpenMP::OpenMP_CXX OpenGL::GL imgui implot imgui-backends range-v3 mpsc)

if (APPLE)
  target_link_directories(${PROJECT_NAME} PRIVATE /Library/Frameworks/PicoSDK.framework/Libraries/libps2000)
  target_include_directories(${PROJECT_NAME} PRIVATE /Library/Frameworks/PicoSDK.framework/Headers)
endif()
