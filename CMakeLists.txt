cmake_minimum_required(VERSION 3.15...3.25)
project(RainEngine VERSION 0.1.0 LANGUAGES CXX C)
set(DAWN_DEBUG_BREAK_ON_ERROR TRUE)
set(DEBUG_BREAK_ON_ERROR TRUE)
set(DAWN_BREAK_ON_ERROR TRUE)
set(CMAKE_CXX_FLAGS "-Wno-error ${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS "-Wno-error ${CMAKE_C_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -v")
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

file(GLOB IMGUI_SOURCES 
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends/imgui_impl_glfw.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends/imgui_impl_wgpu.cpp")
  #"${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGuizmo/*.cpp")

list(APPEND SOURCES ${IMGUI_SOURCES})

if(APPLE)
  list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/osx/macos_window.mm")
endif()
if(EMSCRIPTEN)
  add_definitions(-DIMGUI_IMPL_WEBGPU_BACKEND_WGPU)
else()
  add_definitions(-DIMGUI_IMPL_WEBGPU_BACKEND_DAWN)
endif()
add_executable(ReEngine ${SOURCES})
set_target_properties(ReEngine PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  EXPORT_COMPILE_COMMANDS ON
)

#target_include_directories(ReEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb "vendor/imgui" "vendor/ImGuizmo")
target_compile_definitions(ReEngine PRIVATE RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Resources")
#if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wimplicit-const-int-float-conversion)
#endif()
include("cmake/ThirdParty.cmake")

if(NOT EMSCRIPTEN)
  target_include_directories(ReEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb "vendor/imgui")
  target_include_directories(ReEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/dawn/include/tint)
else()
target_include_directories(ReEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb)
endif()

if(EMSCRIPTEN)
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sDISABLE_EXCEPTION_CATCHING=0")
  target_link_options(ReEngine PRIVATE -sUSE_GLFW=3 -sUSE_WEBGPU=1 -sUSE_ZLIB=1 -sASYNCIFY=1 -sALLOW_MEMORY_GROWTH=1 -sWASM_BIGINT=1 -sASSERTIONS -sSTACK_SIZE=1mb --preload-file "${CMAKE_CURRENT_SOURCE_DIR}/Resources")
  set_target_properties(ReEngine PROPERTIES SUFFIX ".html")
endif()
