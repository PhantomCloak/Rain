#pragma once
#include "core/KeyCode.h"
#include "core/MouseCode.h"

#include <glm/glm.hpp>

namespace Rain
{
    class Input
    {
    public:
        static bool IsKeyPressed(KeyCode key);

        static bool IsMouseButtonPressed(MouseCode button);
        static glm::vec2 GetMousePosition();
        static float GetMouseX();
        static float GetMouseY();
    };
} // namespace Rain
