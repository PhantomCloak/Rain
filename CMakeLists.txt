cmake_minimum_required(VERSION 3.1...3.25)
project(RainEngine VERSION 0.1.0 LANGUAGES CXX C)

#include_directories("vendor" "vendor/imgui" "vendor/spdlog/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

file(GLOB IMGUI_SOURCES 
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends/imgui_impl_glfw.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends/imgui_impl_wgpu.cpp")

list(APPEND SOURCES ${IMGUI_SOURCES})

if(APPLE)
  list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/osx/macos_window.mm")
endif()

#set(TARGET_NAME Rengine)
add_executable(ReEngine ${SOURCES})

set_target_properties(ReEngine PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  EXPORT_COMPILE_COMMANDS ON
)


target_include_directories(ReEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb "vendor/imgui")
target_compile_definitions(ReEngine PRIVATE RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Resources")

include("cmake/ThirdParty.cmake")
target_include_directories(ReEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/dawn/include/tint)

if(EMSCRIPTEN)
	target_link_options(ReEngine PRIVATE
		-sUSE_GLFW=3
		-sUSE_WEBGPU=1
		-sASYNCIFY=1
		-sALLOW_MEMORY_GROWTH=1
		-sWASM_BIGINT=1
		-sSTACK_SIZE=1mb
		-O0
		--preload-file "${CMAKE_CURRENT_SOURCE_DIR}/Resources"
		)
	set_target_properties(MyApp PROPERTIES SUFFIX ".html")
endif()

