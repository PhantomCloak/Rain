
#pragma once
#include <iostream>
#include "Log.h"

#ifndef __EMSCRIPTEN__
#include <execinfo.h>
#include <signal.h>

inline void PrintStackTrace() {
    const int maxFrames = 64;
    void* frames[maxFrames];
    int frameCount = backtrace(frames, maxFrames);
    char** symbols = backtrace_symbols(frames, frameCount);
    
    std::cerr << "Stack trace:\n";
    for (int i = 0; i < frameCount; ++i) {
        std::cerr << symbols[i] << "\n";
    }
    free(symbols);
}

#define RN_DEBUG_BREAK() raise(SIGTRAP)

#define RN_CORE_ASSERT_MESSAGE_INTERNAL(...) \
    ::Rain::Log::PrintAssertMessage(::Rain::Log::Type::Core, "Assertion Failed" __VA_OPT__(, ) __VA_ARGS__); \
    PrintStackTrace()

#define RN_ASSERT_MESSAGE_INTERNAL(...) \
    ::Rain::Log::PrintAssertMessage(::Rain::Log::Type::Client, "Assertion Failed" __VA_OPT__(, ) __VA_ARGS__); \
    PrintStackTrace()

#else
// If compiling with Emscripten, disable stack trace and debug break
inline void PrintStackTrace() {}

#define RN_DEBUG_BREAK() 

#define RN_CORE_ASSERT_MESSAGE_INTERNAL(...) 
#define RN_ASSERT_MESSAGE_INTERNAL(...) 

#endif

#define RN_CORE_ASSERT(condition, ...)              \
  {                                                 \
    if (!(condition)) {                             \
      RN_CORE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); \
      RN_DEBUG_BREAK();                             \
    }                                               \
  }
#define RN_ASSERT(condition, ...)              \
  {                                            \
    if (!(condition)) {                        \
      RN_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); \
      RN_DEBUG_BREAK();                        \
    }                                          \
  }

