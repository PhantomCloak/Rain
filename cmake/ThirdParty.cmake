include("cmake/Utils.cmake")
include_directories("vendor" "vendor/spdlog/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Assimp
set(ASSIMP_BUILD_TESTS FALSE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT FALSE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT FALSE)

set(ASSIMP_BUILD_GLTF_IMPORTER TRUE)
set(ASSIMP_BUILD_GLTF2_IMPORTER TRUE)
set(ASSIMP_BUILD_SMD_IMPORTER TRUE)
set(ASSIMP_BUILD_SIB_IMPORTER TRUE)
set(ASSIMP_BUILD_OBJ_IMPORTER TRUE)

add_subdirectory(vendor/assimp)
set(DAWN_FETCH_DEPENDENCIES ON)
set(DAWN_ENABLE_VULKAN ON)
set(TINT_BUILD_SPV_WRITER ON)

# Dawn WebGPU
if(EMSCRIPTEN)
	set(TINT_BUILD_TESTS OFF)
	set(TINT_BUILD_BENCHMARKS OFF)
	set(TINT_BUILD_CMD_TOOLS   OFF)
	set(TINT_BUILD_FUZZERS OFF)
	set(TINT_BUILD_AS_OTHER_OS ON)
	set(TINT_BUILD_MSL_WRITER  OFF)
	set(TINT_ENABLE_BREAK_IN_DEBUGGER OFF)
	set(TINT_ENABLE_INSTALL ON)
	set(TINT_BUILD_WGSL_READER ON)

	set(TINT_ROOT_SOURCE_DIR vendor/dawn)
	include_directories(${TINT_ROOT_SOURCE_DIR})

	add_subdirectory(vendor/dawn/third_party/abseil-cpp)
	# TODO: Since we don't need most of the stuff from tint, we need to manually turn them off
	add_subdirectory(vendor/dawn/src/tint)
else()
	add_compile_definitions(DAWN_DEBUG_BREAK_ON_ERROR) # TODO: Inspect target?
  add_subdirectory(vendor/dawn)
  #set_target_properties(webgpu_dawn PROPERTIES POSITION_INDEPENDENT_CODE ON)
	message(WARN "Dawn Loading Brrrr Dream")
endif()

# PhysX
#add_subdirectory(vendor/PhysX/physx/compiler/public)

if(EMSCRIPTEN)
	message(FATAL "OH NOOOO!!!")
	target_link_options(PhysX PRIVATE -sALLOW_BLOCKING_ON_MAIN_THREAD=1)

	set(EMSCRIPTEN_PTHREADS_COMPILER_FLAGS "-pthread")
	set(EMSCRIPTEN_PTHREADS_LINKER_FLAGS "${EMSCRIPTEN_PTHREADS_COMPILER_FLAGS}") # -sPROXY_TO_PTHREAD

	string(APPEND CMAKE_C_FLAGS " ${EMSCRIPTEN_PTHREADS_COMPILER_FLAGS}")
	string(APPEND CMAKE_CXX_FLAGS " ${EMSCRIPTEN_PTHREADS_COMPILER_FLAGS}")
	string(APPEND CMAKE_EXE_LINKER_FLAGS " ${EMSCRIPTEN_PTHREADS_LINKER_FLAGS}")
endif()

# ImGui
#set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui")
#add_library(imgui STATIC
#            ${IMGUI_DIR}/imgui.h
#            ${IMGUI_DIR}/imconfig.h
#            ${IMGUI_DIR}/imgui_internal.h
#            ${IMGUI_DIR}/imstb_rectpack.h
#            ${IMGUI_DIR}/imstb_textedit.h
#            ${IMGUI_DIR}/imstb_truetype.h
#            ${IMGUI_DIR}/imgui.cpp
#            ${IMGUI_DIR}/imgui_draw.cpp
#            ${IMGUI_DIR}/imgui_widgets.cpp
#            ${IMGUI_DIR}/imgui_tables.cpp
#            ${IMGUI_DIR}/backends/imgui_impl_glfw.h
#            ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
#            ${IMGUI_DIR}/backends/imgui_impl_wgpu.h
#            ${IMGUI_DIR}/backends/imgui_impl_wgpu.cpp)
#
#
#target_include_directories(imgui PUBLIC $<INSTALL_INTERFACE:${IMGUI_DIR}> $<BUILD_INTERFACE:${IMGUI_DIR}>)
#target_link_libraries(imgui PRIVATE webgpu_dawn webgpu_cpp webgpu_glfw glfw)
#
# Tracy
#add_library(TracyClient STATIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tracy/public/TracyClient.cpp)
#target_include_directories(TracyClient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tracy/public/tracy)
#target_compile_definitions(TracyClient PUBLIC TRACY_ENABLE=1)

# Standalone
add_subdirectory(vendor/spdlog)
add_subdirectory(vendor/flecs)


# Linking
if(EMSCRIPTEN)
	target_link_libraries(ReEngine PRIVATE tint_lang_wgsl_inspector libtint)
else()
	target_link_libraries(ReEngine PRIVATE dawn_common
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
	#imgui
	#TracyClient
	glfw
	assimp
	#PhysX
	#PhysXCooking 
	#PhysXCommon
	)
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
