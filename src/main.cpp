#include "Application.h"

using namespace Rain;

Rain::Window* appi;

int main(int argc, char** argv)
{
  int resX = 1280;
  int resY = 720;

  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--resX") == 0 && i + 1 < argc)
    {
      resX = std::atoi(argv[++i]);
    }
    else if (strcmp(argv[i], "--resY") == 0 && i + 1 < argc)
    {
      resY = std::atoi(argv[++i]);
    }
  }

  Rain::WindowProps props("Bouncing Balls", resX, resY);
  Rain::Window* app = new Application(props);

  app->OnStart();
  appi = app;

  while (true)
  {
    app->OnUpdate();
  }
  return 0;
}
