add_library(imgui 
  imgui/imgui.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  )
target_include_directories(imgui PUBLIC imgui)

add_library(imgui-backends
  imgui/backends/imgui_impl_glfw.cpp
  imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui-backends PUBLIC imgui imgui/backends)
target_link_libraries(imgui-backends PUBLIC glfw imgui)


add_library(implot
  implot/implot.cpp
  implot/implot_items.cpp)
target_include_directories(implot PUBLIC implot imgui)
target_link_libraries(implot PUBLIC imgui)
