#ifdef RN_ENABLE_PROFILING
#include "Profiler.h"
#include "Tracy.hpp"

void* operator new(std::size_t count) {
  auto ptr = malloc(count);
#ifdef RN_PROFILE_MEM_ENABLED
#ifdef RN_PROFILE_MEM_STACK
  TracyAllocS(ptr, count, RN_PROFILE_MEM_STACK_DEPTH);
#else
  TracyAlloc(ptr, count);
#endif
#endif
  return ptr;
}

void operator delete(void* ptr) noexcept {
#ifdef RN_PROFILE_MEM_ENABLED
#ifdef RN_PROFILE_MEM_STACK
  TracyFreeS(ptr, RN_PROFILE_MEM_STACK_DEPTH);
#else
  TracyFree(ptr);
#endif
#endif
  free(ptr);
}
#endif
