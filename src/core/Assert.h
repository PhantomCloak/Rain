#pragma once
#include "Log.h"

#if defined(HZ_PLATFORM_WINDOWS)
#define RN_DEBUG_BREAK() __debugbreak()
#elif defined(RN_PLATFORM_LINUX)
#include <signal.h>
#define RN_DEBUG_BREAK() raise(SIGTRAP)
#elif defined(__APPLE__)  // Adding this line for macOS
#define RN_DEBUG_BREAK() __builtin_trap()
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#define RN_DEBUG_BREAK() EM_ASM({ debugger; });
#else
#error "Platform doesn't support debugbreak yet!"
#endif

#define RN_CORE_ASSERT_MESSAGE_INTERNAL(...) ::Rain::Log::PrintAssertMessage(::Rain::Log::Type::Core, "Assertion Failed" __VA_OPT__(, ) __VA_ARGS__)
#define RN_ASSERT_MESSAGE_INTERNAL(...) ::Rain::Log::PrintAssertMessage(::Rain::Log::Type::Client, "Assertion Failed" __VA_OPT__(, ) __VA_ARGS__)

#define RN_CORE_ASSERT(condition, ...)              \
  {                                                 \
    if (!(condition)) {                             \
      RN_CORE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); \
      RN_DEBUG_BREAK();                               \
    }                                               \
  }
#define RN_ASSERT(condition, ...)              \
  {                                            \
    if (!(condition)) {                        \
      RN_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); \
      RN_DEBUG_BREAK();                          \
    }                                          \
  }
