
cmake_minimum_required(VERSION 3.19...3.25)

if(APPLE)
  project(RainEngine VERSION 0.1.0 LANGUAGES CXX C OBJC OBJCXX)
else()
  project(RainEngine VERSION 0.1.0 LANGUAGES CXX C)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")

file(GLOB IMGUI_SOURCES
  "vendor/imgui/*.cpp"
  "vendor/imgui/backends/imgui_impl_glfw.cpp"
  "vendor/imgui/backends/imgui_impl_wgpu.cpp"
  "vendor/ImGuizmo/*.cpp"
)
list(APPEND SOURCES ${IMGUI_SOURCES})

if(APPLE)
  list(APPEND SOURCES "src/platform/osx/macos_window.mm")
  list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/render/glfw3webgpu.c")
  set_source_files_properties("vendor/imgui/backends/imgui_impl_wgpu.cpp" PROPERTIES LANGUAGE OBJCXX)
endif()

add_executable(ReEngine ${SOURCES})

target_compile_definitions(ReEngine PRIVATE
  RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Resources"
  $<IF:$<BOOL:${EMSCRIPTEN}>,IMGUI_IMPL_WEBGPU_BACKEND_WGPU,IMGUI_IMPL_WEBGPU_BACKEND_DAWN>
)

include(cmake/ThirdParty.cmake)

target_include_directories(ReEngine SYSTEM PRIVATE
  vendor/stb
  vendor/imgui
  vendor/ImGuizmo
)

target_link_libraries(ReEngine PRIVATE
  assimp
  flecs_static
  spdlog
  Jolt
  glm
  ozz_animation_offline
)

if(EMSCRIPTEN)
  target_link_options(ReEngine PRIVATE
    -sUSE_GLFW=3
    -sUSE_WEBGPU=1
    -sUSE_ZLIB=1
    -sASYNCIFY=1
    -sALLOW_MEMORY_GROWTH=1
    -sWASM_BIGINT=1
    -sASSERTIONS
    -sSTACK_SIZE=1mb
    --preload-file "${CMAKE_CURRENT_SOURCE_DIR}/Resources"
  )
  set_target_properties(ReEngine PROPERTIES SUFFIX ".html")
endif()

if(NOT EMSCRIPTEN)
  target_link_libraries(ReEngine PRIVATE
    dawn_glfw
    dawn_native
    dawn_proc
    dawncpp
    TracyClient
    glfw
  )
endif()