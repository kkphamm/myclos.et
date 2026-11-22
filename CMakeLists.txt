cmake_minimum_required(VERSION 3.24)
project(mycloset VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_library(cloth
  src/simulation/avatar.cpp
  src/simulation/measurements.cpp
  src/simulation/pattern.cpp
  src/simulation/limb_patterns.cpp
  src/simulation/cloth.cpp
  src/simulation/contact.cpp
  src/simulation/outfit_contact.cpp
)
target_include_directories(cloth PUBLIC src src/simulation)

add_executable(mycloset_bench tools/bench.cpp)
target_link_libraries(mycloset_bench PRIVATE cloth)
target_compile_definitions(mycloset_bench PRIVATE MYCLOSET_BUILD_TYPE="$<CONFIG>")
include(CTest)
if(BUILD_TESTING)
  add_test(NAME cloth_and_cache COMMAND mycloset_bench --check)
  add_executable(mycloset_form_check tests/form_measurements.cpp)
  target_link_libraries(mycloset_form_check PRIVATE cloth)
  add_test(NAME visible_body_measurements COMMAND mycloset_form_check)
  add_executable(mycloset_fit_check tests/fit_check.cpp)
  target_link_libraries(mycloset_fit_check PRIVATE cloth)
  add_test(NAME garment_body_surface COMMAND mycloset_fit_check)
  add_test(NAME garment_body_edits COMMAND mycloset_fit_check --edits)
  add_test(NAME layered_outfits COMMAND mycloset_fit_check --outfits)
endif()

option(BUILD_VIEWER "Build the macOS OpenGL fitting room" ON)
if(BUILD_VIEWER)
  if(NOT APPLE)
    message(FATAL_ERROR "The desktop viewer currently targets macOS. Use -DBUILD_VIEWER=OFF for the portable CPU benchmark.")
  endif()
  include(FetchContent)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(glfw URL https://codeload.github.com/glfw/glfw/tar.gz/refs/tags/3.4
    URL_HASH SHA256=c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01)
  FetchContent_Declare(imgui URL https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.91.9b
    URL_HASH SHA256=8e1bbc76c71d74fef2fb85db7e7ca8eba13d6a86623c54992b60162db554ffdb)
  FetchContent_MakeAvailable(glfw imgui)
  add_library(dear_imgui STATIC ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
  target_include_directories(dear_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(dear_imgui PUBLIC glfw "-framework OpenGL")
  add_executable(mycloset MACOSX_BUNDLE
    src/main.cpp
    src/viewer/fitting_room.cpp
    src/viewer/renderer.cpp
    src/viewer/capture.cpp
    src/ui/ui.cpp
    src/ui/fitting_view.cpp
    src/ui/closet_view.cpp
    src/ui/fit_controls.cpp
  )
  set_target_properties(mycloset PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "myclos.et"
    MACOSX_BUNDLE_GUI_IDENTIFIER "local.mycloset.fittingroom")
  target_compile_definitions(mycloset PRIVATE GL_SILENCE_DEPRECATION GLFW_INCLUDE_NONE)
  target_link_libraries(mycloset PRIVATE cloth dear_imgui "-framework CoreGraphics" "-framework ImageIO" "-framework CoreFoundation")
endif()
