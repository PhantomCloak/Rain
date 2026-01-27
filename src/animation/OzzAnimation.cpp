#include "OzzAnimation.h"

namespace Rain
{
  void OzzAnimation::SetAnimation(ozz::unique_ptr<ozz::animation::Animation> animation)
  {
    m_Animation = std::move(animation);
    if (m_Animation)
    {
      m_Name = m_Animation->name();
    }
  }

}  // namespace Rain
