#pragma once

#include <string>

#include <ozz/animation/runtime/animation.h>
#include <ozz/base/memory/unique_ptr.h>

namespace Rain
{
  class OzzAnimation
  {
   public:
    OzzAnimation() = default;
    ~OzzAnimation() = default;

    OzzAnimation(const OzzAnimation&) = delete;
    OzzAnimation& operator=(const OzzAnimation&) = delete;
    OzzAnimation(OzzAnimation&&) = default;
    OzzAnimation& operator=(OzzAnimation&&) = default;

    void SetAnimation(ozz::unique_ptr<ozz::animation::Animation> animation);

    const ozz::animation::Animation* GetOzzAnimation() const { return m_Animation.get(); }
    ozz::animation::Animation* GetOzzAnimation() { return m_Animation.get(); }

    float GetDuration() const { return m_Animation ? m_Animation->duration() : 0.0f; }
    int GetNumTracks() const { return m_Animation ? m_Animation->num_tracks() : 0; }

    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

   private:
    ozz::unique_ptr<ozz::animation::Animation> m_Animation;
    std::string m_Name;
  };

}  // namespace Rain
