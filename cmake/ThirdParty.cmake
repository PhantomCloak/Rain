target_include_directories(WebEngine PRIVATE src)

target_include_directories(WebEngine SYSTEM PRIVATE
  vendor/stb
  vendor/imgui
  vendor/ImGuizmo
  vendor/miniz
  vendor/protozero/include
)

if(EMSCRIPTEN)
  set(EMSCRIPTEN_PTHREADS_FLAGS "-pthread")

  string(APPEND CMAKE_C_FLAGS          " ${EMSCRIPTEN_PTHREADS_FLAGS}")
  string(APPEND CMAKE_CXX_FLAGS        " ${EMSCRIPTEN_PTHREADS_FLAGS}")
  string(APPEND CMAKE_EXE_LINKER_FLAGS " ${EMSCRIPTEN_PTHREADS_FLAGS}")

  set(CMAKE_THREAD_LIBS_INIT         "-pthread" CACHE STRING "" FORCE)
  set(CMAKE_HAVE_THREADS_LIBRARY     1          CACHE BOOL   "" FORCE)
  set(CMAKE_USE_WIN32_THREADS_INIT   0          CACHE BOOL   "" FORCE)
  set(CMAKE_USE_PTHREADS_INIT        1          CACHE BOOL   "" FORCE)
  set(THREADS_PREFER_PTHREAD_FLAG    ON         CACHE BOOL   "" FORCE)
endif()

add_subdirectory(vendor/assimp            SYSTEM)
add_subdirectory(vendor/JoltPhysics/Build SYSTEM)
add_subdirectory(vendor/ozz-animation     SYSTEM)
add_subdirectory(vendor/spdlog            SYSTEM)
add_subdirectory(vendor/flecs             SYSTEM)
add_subdirectory(vendor/glm               SYSTEM)
add_subdirectory(vendor/KTX-Software      SYSTEM)

target_include_directories(assimp PRIVATE "${CMAKE_BINARY_DIR}")
target_compile_options(assimp PRIVATE
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-error>
  $<$<C_COMPILER_ID:Clang>:-Wimplicit-const-int-float-conversion>
  $<$<CXX_COMPILER_ID:Clang>:-Wimplicit-const-int-float-conversion>
)

if(EMSCRIPTEN)
  target_compile_definitions(assimp PRIVATE ASSIMP_BUILD_NO_GLTF1_IMPORTER)
endif()

if(NOT EMSCRIPTEN)
  add_subdirectory(vendor/dawn SYSTEM)

  add_library(TracyClient STATIC vendor/tracy/public/TracyClient.cpp)
  target_include_directories(TracyClient PUBLIC vendor/tracy/public/tracy)
  target_compile_definitions(TracyClient PUBLIC TRACY_ENABLE=1)
endif()


