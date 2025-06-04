#include <iostream>
#include <ostream>
#include "Application.h"

using namespace Rain;

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif
Rain::Window* appi;
int main(int, char**) {
  Rain::WindowProps props("Bouncing Balls", 1920 * 2, 1080 * 2);

#if __EMSCRIPTEN__
  double canvasWidth, canvasHeight;
  emscripten_get_element_css_size("#canvas", &canvasWidth, &canvasHeight);
  props.Width = canvasWidth * emscripten_get_device_pixel_ratio();
  props.Height = canvasHeight * emscripten_get_device_pixel_ratio();
#endif

  if (props.Height < 500 || props.Height < 500) {
    props.Height = 1080;
    props.Width = 1920;
  }

  Rain::Window* app = new Application(props);
  app->OnStart();
  appi = app;
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(
      [](void* userData) {
        Application& app = *reinterpret_cast<Application*>(userData);
        appi->OnUpdate();
      },
      (void*)&app,
      0, true);
#else   // __EMSCRIPTEN__
  while (true) {
    app->OnUpdate();
  }
#endif  // __EMSCRIPTEN__

  return 0;
}
