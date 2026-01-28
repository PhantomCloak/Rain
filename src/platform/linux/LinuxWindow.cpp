#include "LinuxWindow.h"
#include "core/Assert.h"
#include "core/Log.h"

static void GLFWErrorCallback(int error, const char* description) {
  RN_LOG_ERR("GLFW Error ({0}): {1}", error, description);
}

Rain::LinuxWindow::~LinuxWindow() {
  Shutdown();
}

void Rain::LinuxWindow::Init(const WindowProps& props) {
  //Rain::Log::Init();
  //RN_CORE_INFO("Creating window {0} ({1}, {2})", props.Title, props.Width, props.Height);

  bool success = glfwInit();
  //RN_CORE_ASSERT(success, "Could not initialize GLFW!");

  glfwSetErrorCallback(GLFWErrorCallback);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, "WGPU!", nullptr, nullptr);

	RN_ASSERT(m_Window, "Window cannot be created!");


  glfwSetWindowUserPointer(m_Window, this);

  glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) {
			// TODO: Propagate event to gracefully stop
			exit(0);
  });

  glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos) {
    LinuxWindow* linuxWindow = static_cast<LinuxWindow*>(glfwGetWindowUserPointer(window));
    linuxWindow->OnMouseMove(xPos, yPos);
  });

  glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
    LinuxWindow* linuxWindow = static_cast<LinuxWindow*>(glfwGetWindowUserPointer(window));
    switch (action) {
      case GLFW_PRESS: {
        linuxWindow->OnKeyPressed(key, Key::RN_KEY_PRESS);
        break;
      }
      case GLFW_RELEASE: {
        linuxWindow->OnKeyPressed(key, Key::RN_KEY_RELEASE);
        break;
      }
      case GLFW_REPEAT: {
        linuxWindow->OnKeyPressed(key, Key::RN_KEY_REPEAT);
        break;
      }
    }
  });

  glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods) {
    LinuxWindow* linuxWindow = static_cast<LinuxWindow*>(glfwGetWindowUserPointer(window));
    switch (action) {
      case GLFW_PRESS: {
        linuxWindow->OnMouseClick(Mouse::Press);
        break;
      }
      case GLFW_RELEASE: {
        linuxWindow->OnMouseClick(Mouse::Release);
        break;
      }
    }
  });

  glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
    LinuxWindow* linuxWindow = static_cast<LinuxWindow*>(glfwGetWindowUserPointer(window));
    linuxWindow->OnResize(height, width);
  });
}

void Rain::LinuxWindow::Shutdown() {
}
