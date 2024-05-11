cmake_minimum_required(VERSION 3.1...3.25)
project(MyDawnProject VERSION 0.1.0 LANGUAGES CXX C)

include_directories("vendor" "vendor/imgui" "vendor/spdlog/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

file(GLOB IMGUI_SOURCES 
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends/imgui_impl_glfw.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends/imgui_impl_wgpu.cpp")

if(APPLE)
  list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/osx/macos_window.mm")
endif()

list(APPEND SOURCES ${IMGUI_SOURCES})

add_executable(MyApp ${SOURCES})

set_target_properties(MyApp PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  EXPORT_COMPILE_COMMANDS ON
)

add_subdirectory(vendor/spdlog)
add_subdirectory(vendor/assimp)

if(DEV_MODE)
  target_compile_definitions(MyApp PRIVATE RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Resources")
else()
  target_compile_definitions(MyApp PRIVATE RESOURCE_DIR="./Resources")
endif()

target_link_libraries(MyApp PRIVATE spdlog assimp)

if(EMSCRIPTEN)
  target_link_options(MyApp PRIVATE
    -sUSE_GLFW=3
    -sUSE_WEBGPU=1
    -sASYNCIFY=1
    -sALLOW_MEMORY_GROWTH
    -sWASM_BIGINT=1
    -sSTACK_SIZE=1mb
    -O0
    -g3
    --source-map
    --emit-symbol-map
    --preload-file "${CMAKE_CURRENT_SOURCE_DIR}/Resources"
  )
  set_target_properties(MyApp PROPERTIES SUFFIX ".html")
else()
  add_subdirectory(vendor/dawn)
  target_link_libraries(MyApp PRIVATE dawn_common dawn_glfw dawn_headers dawn_native dawn_platform dawn_utils dawn_wire dawncpp dawncpp_headers webgpu_dawn libtint glfw)
  target_link_libraries(MyApp PRIVATE "-framework Cocoa" "-framework CoreVideo" "-framework IOKit" "-framework QuartzCore")
endif()

