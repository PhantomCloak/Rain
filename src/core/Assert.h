
#pragma once
#include <iostream>
#include "Log.h"

#ifndef __EMSCRIPTEN__


#ifdef __linux__
#include <execinfo.h>
#include <signal.h>
#endif


inline void PrintStackTrace() {
#ifdef __linux__
    const int maxFrames = 64;
    void* frames[maxFrames];
    int frameCount = backtrace(frames, maxFrames);
    char** symbols = backtrace_symbols(frames, frameCount);

    std::cerr << "Stack trace:\n";
    for (int i = 0; i < frameCount; ++i) {
        std::cerr << symbols[i] << "\n";
    }
    free(symbols);
#endif
}

#if defined(_WIN32)
#define RN_DEBUG_BREAK() __debugbreak()
#elif defined(__linux__) || defined(__APPLE__)
#define RN_DEBUG_BREAK() raise(SIGTRAP)
#else
#define RN_DEBUG_BREAK()
#endif

#define RN_CORE_ASSERT_MESSAGE_INTERNAL(...) \
    ::WebEngine::Log::PrintAssertMessage(::WebEngine::Log::Type::Core, "Assertion Failed" __VA_OPT__(, ) __VA_ARGS__); \
    PrintStackTrace()

#define RN_ASSERT_MESSAGE_INTERNAL(...) \
    ::WebEngine::Log::PrintAssertMessage(::WebEngine::Log::Type::Client, "Assertion Failed" __VA_OPT__(, ) __VA_ARGS__); \
    PrintStackTrace()

#else
// If compiling with Emscripten, disable stack trace and debug break
inline void PrintStackTrace() {}

#define RN_DEBUG_BREAK() 

#define RN_CORE_ASSERT_MESSAGE_INTERNAL(...) 
#define RN_ASSERT_MESSAGE_INTERNAL(...) 

#endif

#if defined(_MSC_VER)
#define RN_ASSUME_UNREACHABLE() __assume(0)
#elif defined(__GNUC__) || defined(__clang__)
#define RN_ASSUME_UNREACHABLE() __builtin_unreachable()
#else
#define RN_ASSUME_UNREACHABLE()
#endif

#define RN_CORE_ASSERT(condition, ...)              \
  {                                                 \
    if (!(condition)) {                             \
      RN_CORE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); \
      RN_DEBUG_BREAK();                             \
      RN_ASSUME_UNREACHABLE();                      \
    }                                               \
  }
#define RN_ASSERT(condition, ...)              \
  {                                            \
    if (!(condition)) {                        \
      RN_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); \
      RN_DEBUG_BREAK();                        \
      RN_ASSUME_UNREACHABLE();                 \
    }                                          \
  }

