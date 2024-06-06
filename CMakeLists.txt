
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

# Include dependencies
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT FALSE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT FALSE)

set(ASSIMP_BUILD_GLTF_IMPORTER TRUE)
set(ASSIMP_BUILD_GLTF2_IMPORTER TRUE)
set(ASSIMP_BUILD_SMD_IMPORTER TRUE)
set(ASSIMP_BUILD_SIB_IMPORTER TRUE)
set(ASSIMP_BUILD_OBJ_IMPORTER TRUE)

add_subdirectory(vendor/PhysX/physx/compiler/public)
if(DEV_MODE)
  target_compile_definitions(MyApp PRIVATE RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Resources")
else()
  target_compile_definitions(MyApp PRIVATE RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Resources")
endif()


add_subdirectory(vendor/spdlog)
add_subdirectory(vendor/assimp)
add_subdirectory(vendor/flecs)

if(EMSCRIPTEN)

	# PhysX Needs It
	#set(EMSCRIPTEN_PTHREADS_COMPILER_FLAGS "-pthread")
	#set(EMSCRIPTEN_PTHREADS_LINKER_FLAGS "${EMSCRIPTEN_PTHREADS_COMPILER_FLAGS}") # -sPROXY_TO_PTHREAD

	#string(APPEND CMAKE_C_FLAGS " ${EMSCRIPTEN_PTHREADS_COMPILER_FLAGS}")
	#string(APPEND CMAKE_CXX_FLAGS " ${EMSCRIPTEN_PTHREADS_COMPILER_FLAGS}")
	#string(APPEND CMAKE_EXE_LINKER_FLAGS " ${EMSCRIPTEN_PTHREADS_LINKER_FLAGS}")

	target_link_options(PhysX PRIVATE -sALLOW_BLOCKING_ON_MAIN_THREAD=1)

	target_link_libraries(MyApp PRIVATE assimp PhysX PhysXCooking PhysXCommon PhysXExtensions)
	target_link_options(MyApp PRIVATE
		-sUSE_GLFW=3
		-sUSE_WEBGPU=1
		-sASYNCIFY=1
		-sALLOW_MEMORY_GROWTH=1
		#-sMAXIMUM_MEMORY=1GB
		-sWASM_BIGINT=1
		-sSTACK_SIZE=1mb
		#-sPTHREAD_POOL_SIZE=4
		#-sALLOW_BLOCKING_ON_MAIN_THREAD=1
		-O0
		--preload-file "${CMAKE_CURRENT_SOURCE_DIR}/Resources"
		)
	set_target_properties(MyApp PROPERTIES SUFFIX ".html")
else()
  add_subdirectory(vendor/dawn)
  add_compile_definitions(DAWN_DEBUG_BREAK_ON_ERROR)
  target_link_libraries(MyApp PRIVATE dawn_common
    dawn_glfw
    dawn_headers
    dawn_native
    dawn_platform
    dawn_utils
    dawn_wire
    dawncpp
    dawncpp_headers
    webgpu_dawn
    libtint
		spdlog
		flecs
	glfw
	assimp
	PhysX
	PhysXCooking 
	PhysXCommon 
	PhysXExtensions
	"-framework Cocoa" "-framework CoreVideo" "-framework IOKit" "-framework QuartzCore")
endif()

