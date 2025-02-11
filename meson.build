project('simple', 'cpp', default_options: ['cpp_std=c++20'])

src = ['src/main.cpp', 'src/pico.cpp', 'src/processing.cpp', 'src/ui.cpp']
includedirs = include_directories('include', 'mpsc')
mpsc_inc = include_directories('mpsc')

openmp = dependency('openmp')
imgui = dependency('imgui')
implot = dependency('implot')
fftw = dependency('fftw3')
opengl = dependency('gl')
gtest = dependency('gtest')
range = dependency('range-v3')

cc = meson.get_compiler('cpp')
ps2000 = cc.find_library('ps2000')
gtest_main = cc.find_library('gtest_main')

deps = [imgui, implot, fftw,opengl, ps2000, range, openmp]

mpsc_test = executable('mpsc-test', 'mpsc/test.cpp', include_directories: mpsc_inc, dependencies : [gtest, gtest_main])
test('mpsc-test', mpsc_test)

executable('imgui-test', src, include_directories: includedirs, dependencies : deps)
