project('simple', 'cpp')

src = ['src/main.cpp', 'src/pico.cpp', 'src/processing.cpp']
includedirs = include_directories('include')

imgui = dependency('imgui')
implot = dependency('implot')
fftw = dependency('fftw3')
opengl = dependency('gl')

cc = meson.get_compiler('cpp')
ps2000 = cc.find_library('ps2000')

deps = [imgui, implot, fftw,opengl, ps2000]

executable('imgui-test', src, include_directories: includedirs, dependencies : deps)
