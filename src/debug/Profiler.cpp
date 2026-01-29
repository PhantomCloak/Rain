#include "Profiler.h"

#if RN_ENABLE_PROFILING
#include "Tracy.hpp"

static bool g_tracyReady = false;

void InitTracyMemoryProfiling()
{
  g_tracyReady = true;
}

void* operator new(std::size_t count)
{
  auto ptr = malloc(count);
#ifdef RN_PROFILE_MEM_ENABLED
  if (g_tracyReady)
  {
#ifdef RN_PROFILE_MEM_STACK
    TracyAllocS(ptr, count, RN_PROFILE_MEM_STACK_DEPTH);
#else
    TracyAlloc(ptr, count);
#endif
  }
#endif
  return ptr;
}

void operator delete(void* ptr) noexcept
{
#ifdef RN_PROFILE_MEM_ENABLED
  if (g_tracyReady)
  {
#ifdef RN_PROFILE_MEM_STACK
    TracyFreeS(ptr, RN_PROFILE_MEM_STACK_DEPTH);
#else
    TracyFree(ptr);
#endif
  }
#endif
  free(ptr);
}
#endif
