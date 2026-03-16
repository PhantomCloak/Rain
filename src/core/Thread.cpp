#include "core/Thread.h"

#if __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

void Thread::Sleep(unsigned int ms)
{
#if __EMSCRIPTEN__
  emscripten_sleep(ms);
#elif defined(_WIN32)
  ::Sleep(ms);
#else
  usleep(ms * 1000);
#endif
}
