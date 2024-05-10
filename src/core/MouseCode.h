#pragma once
#include <cstdint>

namespace Rain {
  using MouseCode = uint16_t;
  using MouseState = uint16_t;

  namespace Mouse {

    enum : MouseState {
      // From glfw3.h
      Release = 0,
      Press = 1,
      Repeat = 2
    };

    enum : MouseCode {
      // From glfw3.h
      Button0 = 0,
      Button1 = 1,
      Button2 = 2,
      Button3 = 3,
      Button4 = 4,
      Button5 = 5,
      Button6 = 6,
      Button7 = 7,

      ButtonLast = Button7,
      ButtonLeft = Button0,
      ButtonRight = Button1,
      ButtonMiddle = Button2
    };
  }  // namespace Mouse
}  // namespace Rain
