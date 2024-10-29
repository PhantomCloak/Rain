#pragma once

#define RN_ENABLE_PROFILING 1

#define RN_PROFILE_MEM_ENABLED TRUE
//#define RN_PROFILE_MEM_STACK FALSE
#define RN_PROFILE_MEM_STACK_DEPTH 3

#if RN_ENABLE_PROFILING
#include <Tracy.hpp>
void* operator new(std::size_t count);
void operator delete(void* ptr) noexcept;
#endif

#if RN_ENABLE_PROFILING
#define RN_PROFILE_MARK_FRAME FrameMark;
#define RN_PROFILE_FUNC ZoneScoped;
#define RN_PROFILE_SCOPE(...) RN_PROFILE_FUNC(__VA_ARGS__)
#define RN_PROFILE_FUNCN(name) ZoneScopedN(name)
#else
#define RN_PROFILE_MARK_FRAME
#define RN_PROFILE_FUNC
#define RN_PROFILE_FUNCN(name)
#endif
