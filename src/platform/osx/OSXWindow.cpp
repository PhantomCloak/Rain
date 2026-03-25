#include "OSXWindow.h"
#include "core/Assert.h"
#include "core/Log.h"

static void GLFWErrorCallback(int error, const char* description)
{
  RN_LOG_ERR("GLFW Error ({0}): {1}", error, description);
}

WebEngine::OSXWindow::~OSXWindow()
{
  Shutdown();
}

void WebEngine::OSXWindow::Init(const WindowProps& props)
{
  // WebEngine::Log::Init();
  // RN_CORE_INFO("Creating window {0} ({1}, {2})", props.Title, props.Width, props.Height);

  bool success = glfwInit();
  // RN_CORE_ASSERT(success, "Could not initialize GLFW!");

  glfwSetErrorCallback(GLFWErrorCallback);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, "WGPU!", nullptr, nullptr);

  RN_ASSERT(m_Window, "Window cannot be created!");

  glfwSetWindowUserPointer(m_Window, this);

  glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
                             {
			// TODO: Propagate event to gracefully stop
			exit(0); });

  glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
                           {
    OSXWindow* osxWindow = static_cast<OSXWindow*>(glfwGetWindowUserPointer(window));
    osxWindow->OnMouseMove(xPos, yPos); });

  glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
                     {
    OSXWindow* osxWindow = static_cast<OSXWindow*>(glfwGetWindowUserPointer(window));
    switch (action) {
      case GLFW_PRESS: {
        osxWindow->OnKeyPressed(key, Key::RN_KEY_PRESS);
        break;
      }
      case GLFW_RELEASE: {
        osxWindow->OnKeyPressed(key, Key::RN_KEY_RELEASE);
        break;
      }
      case GLFW_REPEAT: {
        osxWindow->OnKeyPressed(key, Key::RN_KEY_REPEAT);
        break;
      }
      default:
      RN_LOG_ERR("Received key callback with unknown type");
      break;
    } });

  glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
                                 {
    OSXWindow* osxWindow = static_cast<OSXWindow*>(glfwGetWindowUserPointer(window));
    osxWindow->OnResize(height, width); });
}

void WebEngine::OSXWindow::Shutdown()
{
}
