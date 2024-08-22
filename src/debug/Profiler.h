#pragma once

#define RN_ENABLE_PROFILING 1

#if RN_ENABLE_PROFILING 
#include <Tracy.hpp>
#endif

#if RN_ENABLE_PROFILING
#define RN_PROFILE_MARK_FRAME			FrameMark;
#define RN_PROFILE_FUNC ZoneScoped;
#define RN_PROFILE_FUNCN(name) ZoneScopedN(name)
#else
#define RN_PROFILE_MARK_FRAME
#define RN_PROFILE_FUNC
#define RN_PROFILE_FUNCN(name)
#endif
