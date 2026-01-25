#include "Application.h"
#include "platform/CommandLine.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

using namespace Rain;

Rain::Window* AppInstance;

int main(int argc, char** argv)
{
  int resX = 1920;
  int resY = 1080;

  std::vector<std::string> strArgs;
  for (int i = 1; i < argc; i++)
  {
    strArgs.push_back(argv[i]);
  }
  CommandLine::InitializeParams(strArgs);

  // if (CommandLine::Param("--resx"))
  //{
  //   resX = CommandLine::Get<int64_t>("--resx");
  // }

  // if (CommandLine::Param("--resy"))
  //{
  //   resY = CommandLine::Get<int64_t>("--resy");
  // }

  // CommandLine::AddParam("--render", "noapi");

  Rain::WindowProps props("Bouncing Balls", resX, resY);
#if EMSCRIPTTEN
  double canvasWidth, canvasHeight;
  emscripten_get_element_css_size("#canvas", &canvasWidth, &canvasHeight);
  props.Width = canvasWidth * emscripten_get_device_pixel_ratio();
  props.Height = canvasHeight * emscripten_get_device_pixel_ratio();

  // RN_LOG("Pixel Ratio: {}", emscripten_get_device_pixel_ratio());
#endif
  RN_LOG("W: {} H: {}", props.Width, props.Height);

  Rain::Window* app = new Application(props);

  app->OnStart();
  AppInstance = app;

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(
      [](void* userData)
      {
        Application& app = *reinterpret_cast<Application*>(userData);
        AppInstance->OnUpdate();
      },
      (void*)&app,
      0, true);
#else   // __EMSCRIPTEN__
  while (true)
  {
    app->OnUpdate();
  }
#endif  // __EMSCRIPTEN__

  return 0;
}
