#include "core/Thread.h"
#include <unistd.h>

#if __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

void Thread::Sleep(unsigned int ms)
{
#if __EMSCRIPTEN__
  emscripten_sleep(ms);
#else
  usleep(ms);
#endif
}
