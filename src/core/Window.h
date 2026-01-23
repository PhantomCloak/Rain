#pragma once
#include <cstdint>
#include <string>

namespace Rain
{
  struct WindowProps
  {
    std::string Title;
    uint32_t Width;
    uint32_t Height;

    WindowProps(const std::string& title = "Rain Engine",
                uint32_t width = 1920,
                uint32_t height = 1080)
        : Title(title), Width(width), Height(height)
    {
    }
  };

  class Window
  {
   public:
    Window() = default;
    virtual ~Window() = default;

    virtual void OnStart() = 0;
    virtual void OnUpdate() = 0;

    // virtual uint32_t GetWidth() const = 0;
    // virtual uint32_t GetHeight() const = 0;

    virtual void* GetNativeWindow() const = 0;
  };
}  // namespace Rain
