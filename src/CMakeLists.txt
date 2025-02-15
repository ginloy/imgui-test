
add_executable(${PROJECT_NAME} main.cpp processing.cpp ui.cpp pico.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw ps2000 OpenMP::OpenMP_CXX OpenGL::GL imgui implot imgui-backends range-v3 mpsc)

if (DEFINED FFTW3_FOUND)
  target_include_directories(${PROJECT_NAME} PRIVATE ${FFTW3_INCLUDE_DIRS})
  target_link_directories(${PROJECT_NAME} PRIVATE ${FFTW3_LIBRARY_DIRS})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${FFTW3_LIBRARIES})
else()
  target_include_directories(${PROJECT_NAME} PRIVATE ${fftw3_SOURCE_DIR}/api)
  target_link_libraries(${PROJECT_NAME} PRIVATE fftw3)
endif()

if (APPLE)
  target_link_directories(${PROJECT_NAME} PRIVATE /Library/Frameworks/PicoSDK.framework/Libraries/libps2000)
  target_include_directories(${PROJECT_NAME} PRIVATE /Library/Frameworks/PicoSDK.framework/Headers)
endif()
