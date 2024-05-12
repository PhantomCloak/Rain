#include "cursor.h"
#include <GLFW/glfw3.h>

#if EDITOR
#include "../../../editor/editor.h"
#endif

static GLFWwindow* NativeWndPtr;
static bool isMouseCapturing = false;

void Cursor::Setup(void* window) {
    NativeWndPtr = (GLFWwindow*)window;
}

glm::vec2 Cursor::GetCursorPosition() {
    double x = 0;
    double y = 0;

    glfwGetCursorPos(NativeWndPtr, &x, &y);
    return glm::vec2(x, y);
}

bool Cursor::HasLeftCursorClicked() {
    return glfwGetMouseButton(NativeWndPtr, 0);
}

bool Cursor::HasRightCursorClicked() {
    return glfwGetMouseButton(NativeWndPtr, 1);
}

void Cursor::CaptureMouse(bool shouldCapture)
{
	if(shouldCapture)
		glfwSetInputMode((GLFWwindow*)NativeWndPtr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	else
		glfwSetInputMode((GLFWwindow*)NativeWndPtr, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	isMouseCapturing = shouldCapture;
}

bool Cursor::IsMouseCaptured()
{
	return isMouseCapturing;
}
