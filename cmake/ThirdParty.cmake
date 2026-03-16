include("cmake/Utils.cmake")
include_directories("vendor" "vendor/spdlog/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=implicit-const-int-float-conversion")
endif()

if(EMSCRIPTEN)
  set(EMSCRIPTEN_PTHREADS_COMPILER_FLAGS "-pthread")
  set(EMSCRIPTEN_PTHREADS_LINKER_FLAGS "${EMSCRIPTEN_PTHREADS_COMPILER_FLAGS}") # -sPROXY_TO_PTHREAD

  string(APPEND CMAKE_C_FLAGS " ${EMSCRIPTEN_PTHREADS_COMPILER_FLAGS}")
  string(APPEND CMAKE_CXX_FLAGS " ${EMSCRIPTEN_PTHREADS_COMPILER_FLAGS}")
  string(APPEND CMAKE_EXE_LINKER_FLAGS " ${EMSCRIPTEN_PTHREADS_LINKER_FLAGS}")

  set(CMAKE_THREAD_LIBS_INIT "-pthread" CACHE STRING "" FORCE)
  set(CMAKE_HAVE_THREADS_LIBRARY 1 CACHE BOOL "" FORCE)
  set(CMAKE_USE_WIN32_THREADS_INIT 0 CACHE BOOL "" FORCE)
  set(CMAKE_USE_PTHREADS_INIT 1 CACHE BOOL "" FORCE)
  set(THREADS_PREFER_PTHREAD_FLAG ON CACHE BOOL "" FORCE)
  add_subdirectory(vendor/assimp)
  target_include_directories(assimp PRIVATE "${CMAKE_BINARY_DIR}")
else()
  add_compile_definitions(DAWN_DEBUG_BREAK_ON_ERROR) # TODO: Inspect target?

  add_subdirectory(vendor/assimp)
  target_include_directories(assimp PRIVATE "${CMAKE_BINARY_DIR}")

  add_subdirectory(vendor/dawn)

  add_library(TracyClient STATIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tracy/public/TracyClient.cpp)
  target_include_directories(TracyClient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tracy/public/tracy)
  target_compile_definitions(TracyClient PUBLIC TRACY_ENABLE=1)
endif()

add_subdirectory(vendor/JoltPhysics/Build)
add_subdirectory(vendor/ozz-animation)
target_include_directories(ReEngine PRIVATE vendor/ozz-animation/include)

add_subdirectory(vendor/spdlog)
add_subdirectory(vendor/flecs)
target_include_directories(ReEngine PRIVATE vendor/JoltPhysics vendor/JoltPhysics/Jolt)

if(EMSCRIPTEN)
	target_link_libraries(ReEngine PRIVATE assimp flecs spdlog ozz_base ozz_animation ozz_animation_offline)
else()
  target_link_libraries(ReEngine PRIVATE dawn_common
  dawn_glfw
  dawn_headers
  dawn_native
  dawn_platform
  dawn_wire
  dawncpp
  dawncpp_headers
  libtint
  dawn_proc
  spdlog
  flecs
  TracyClient
  glfw
  Jolt
  ozz_base
  ozz_animation
  ozz_animation_offline
  assimp)
endif()

# Linking Common
#target_link_libraries(ReEngine PRIVATE
#		assimp
#		flecs
#		spdlog
#		imgui
#		PhysX
#		PhysXCooking
#		PhysXCommon
#		PhysXExtensions)
