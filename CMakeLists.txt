
cmake_minimum_required(VERSION 3.19...3.25)

if(APPLE)
  project(WebEngine VERSION 0.1.0 LANGUAGES CXX C OBJC OBJCXX)
else()
  project(WebEngine VERSION 0.1.0 LANGUAGES CXX C)
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
list(APPEND SOURCES
  "vendor/miniz/miniz.c"
  "vendor/miniz/miniz_tdef.c"
  "vendor/miniz/miniz_tinfl.c"
  "vendor/miniz/miniz_zip.c"
)

if(APPLE)
  list(APPEND SOURCES "src/platform/osx/macos_window.mm")
  list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/render/glfw3webgpu.c")
  set_source_files_properties("vendor/imgui/backends/imgui_impl_wgpu.cpp" PROPERTIES LANGUAGE OBJCXX)
endif()

add_executable(WebEngine ${SOURCES})

target_include_directories(WebEngine PRIVATE
  ${CMAKE_SOURCE_DIR}/vendor/earcut.hpp/include
)

target_compile_options(WebEngine PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:-Wno-changes-meaning>
)

option(USE_GPU_COMPRESSION "Upload KTX2 textures as compressed blocks using device-supported format (BC7/ETC2/ASTC). Requires the corresponding WebGPU device feature to be available at runtime." OFF)

target_compile_definitions(WebEngine PRIVATE
  RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Resources"
  $<IF:$<BOOL:${EMSCRIPTEN}>,IMGUI_IMPL_WEBGPU_BACKEND_WGPU,IMGUI_IMPL_WEBGPU_BACKEND_DAWN>
  $<$<BOOL:${USE_GPU_COMPRESSION}>:USE_GPU_COMPRESSION>
)

include(cmake/ThirdParty.cmake)

# SQLite is used by the map renderer to read .mbtiles tile archives. On
# Emscripten this would need to be vendored and built from source; guard the
# dependency so desktop builds keep using the system package while the wasm
# build path remains untouched until it's needed there.
if(NOT EMSCRIPTEN)
  find_package(SQLite3 REQUIRED)
endif()

target_link_libraries(WebEngine PRIVATE
  assimp
  flecs_static
  spdlog
  Jolt
  glm
  ozz_animation_offline
  ktx_read
)

if(NOT EMSCRIPTEN)
  target_link_libraries(WebEngine PRIVATE SQLite::SQLite3)
endif()

if(EMSCRIPTEN)
  target_link_options(WebEngine PRIVATE
    -sUSE_GLFW=3
    -sUSE_WEBGPU=1
    -sUSE_ZLIB=1
    -sASYNCIFY=1
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=512mb
	-sNO_DISABLE_EXCEPTION_CATCHING
    -sWASM_BIGINT=1
    -sASSERTIONS
    -sSTACK_SIZE=1mb
    -sPTHREAD_POOL_SIZE=4
    -sPTHREAD_POOL_SIZE_STRICT=0
    "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/Resources@Resources"
  )
  set_target_properties(WebEngine PROPERTIES SUFFIX ".html")
endif()

add_custom_command(TARGET WebEngine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources
    $<TARGET_FILE_DIR:WebEngine>/Resources
  COMMENT "Copying Resources to output directory"
)

if(NOT EMSCRIPTEN)
  target_link_libraries(WebEngine PRIVATE
    dawn_glfw
    dawn_native
    dawn_proc
    dawncpp
    TracyClient
    glfw
  )
endif()
