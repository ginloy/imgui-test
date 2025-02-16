
add_library(mpsc INTERFACE)
target_include_directories(mpsc INTERFACE .)

add_executable(mpsc-test test.cpp)
target_compile_options(mpsc-test PRIVATE -fsanitize=thread)
target_link_options(mpsc-test PRIVATE -fsanitize=thread)
target_link_libraries(mpsc-test PRIVATE GTest::gtest_main GTest::gtest)
add_test(
  NAME mpsc-test
  COMMAND mpsc-test
)
