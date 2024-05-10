#include <GLFW/glfw3.h>
#include "core/KeyCode.h"
#include "core/MouseCode.h"
#include "core/Window.h"

namespace Rain {
  class OSXWindow : public Rain::Window {
   protected:
    virtual void OnKeyPressed(KeyCode key, KeyAction action) {};
    virtual void OnMouseMove(double xPos, double yPos) {};
    virtual void OnMouseClick(MouseCode button) {};

    virtual void OnResize(int height, int width) {};

   public:
    OSXWindow(const WindowProps& props) { Init(props);}
    virtual ~OSXWindow();

    void OnStart() override = 0;
    void OnUpdate() override;

    //unsigned int GetWidth() const = 0;
    //unsigned int GetHeight() const = 0;

    virtual void* GetNativeWindow() const override { return m_Window; }

   private:
    virtual void Init(const WindowProps& props);
    virtual void Shutdown();

   private:
    GLFWwindow* m_Window;
  };
}  // namespace Rain
