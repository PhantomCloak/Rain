#include "core/Thread.h"
#include <unistd.h>

void Thread::Sleep(unsigned int ms)
{
#if __EMSCRIPTEN__
  emscripten_sleep(ms);
#else
  usleep(ms);
#endif
}
