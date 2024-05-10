## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##  * Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
##  * Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##  * Neither the name of NVIDIA CORPORATION nor the names of its
##    contributors may be used to endorse or promote products derived
##    from this software without specific prior written permission.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
## EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
## PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
## CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
## EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
## PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
## PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
## OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##
## Copyright (c) 2008-2022 NVIDIA Corporation. All rights reserved.

#
# Build LowLevelDynamics
#

SET(LOWLEVELDYNAMICS_PLATFORM_INCLUDES
	${PHYSX_SOURCE_DIR}/common/src/mac
	${PHYSX_SOURCE_DIR}/lowlevel/software/include/mac
	${PHYSX_SOURCE_DIR}/lowleveldynamics/include/mac
	${PHYSX_SOURCE_DIR}/lowlevel/common/include/pipeline/mac
)

# Use generator expressions to set config specific preprocessor definitions
SET(LOWLEVELDYNAMICS_COMPILE_DEFS

	# Common to all configurations
	${PHYSX_EMSCRIPTEN_COMPILE_DEFS};PX_PHYSX_STATIC_LIB

	$<$<CONFIG:debug>:${PHYSX_EMSCRIPTEN_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_EMSCRIPTEN_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_EMSCRIPTEN_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_EMSCRIPTEN_RELEASE_COMPILE_DEFS};>
)

SET(LOWLEVELDYNAMICS_LIBTYPE OBJECT)

message(STATUS "LLD_DBG_LOWLEVELDYNAMICS_COMPILE_DEFS: ${LOWLEVELDYNAMICS_COMPILE_DEFS}")
message(STATUS "LLD_DBG_UNKWN_FLAG1: ${PHYSX_EMSCRIPTEN_DEBUG_COMPILE_DEFS}")
message(STATUS "LLD_Current C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "LLD_CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "LLD_CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "LLD_CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "LLD_CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")

if(EMSCRIPTEN)
    message(STATUS "Configuring for Emscripten")
else()
    message(STATUS "Not an Emscripten build")
endif()


if(TARGET LowLevelDynamics)
    get_target_property(low_level_dynamics_compile_options LowLevelDynamics COMPILE_OPTIONS)
    message(STATUS "DBG_LowLevelDynamics Compile Options: ${low_level_dynamics_compile_options}")
else()
    message(WARNING "Target LowLevelDynamics not found")
endif()


