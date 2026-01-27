#pragma once

#include <glm/glm.hpp>
#include <vector>

#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/maths/soa_transform.h>

#include "OzzAnimation.h"
#include "OzzSkeleton.h"
#include "core/Ref.h"

namespace Rain
{
  class OzzAnimator
  {
   public:
    OzzAnimator() = default;
    ~OzzAnimator() = default;

    OzzAnimator(const OzzAnimator&) = delete;
    OzzAnimator& operator=(const OzzAnimator&) = delete;

    bool Initialize(Ref<OzzSkeleton> skeleton);

    void SetAnimation(Ref<OzzAnimation> animation);
    Ref<OzzAnimation> GetAnimation() const { return m_Animation; }

    void Update(float deltaTime);

    const std::vector<glm::mat4>& GetBoneMatrices() const { return m_BoneMatrices; }

    float GetPlaybackSpeed() const { return m_PlaybackSpeed; }
    void SetPlaybackSpeed(float speed) { m_PlaybackSpeed = speed; }

    bool IsLooping() const { return m_Loop; }
    void SetLooping(bool loop) { m_Loop = loop; }

    bool IsPlaying() const { return m_Playing; }
    void SetPlaying(bool playing) { m_Playing = playing; }

    void Play() { m_Playing = true; }
    void Pause() { m_Playing = false; }
    void Stop();

    float GetCurrentTime() const { return m_CurrentTime; }
    void SetCurrentTime(float time);

    float GetNormalizedTime() const;

   private:
    void ComputeBoneMatrices();

    Ref<OzzSkeleton> m_Skeleton;
    Ref<OzzAnimation> m_Animation;

    ozz::animation::SamplingJob::Context m_SamplingContext;
    std::vector<ozz::math::SoaTransform> m_LocalTransforms;
    std::vector<ozz::math::Float4x4> m_ModelMatrices;
    std::vector<glm::mat4> m_BoneMatrices;

    float m_CurrentTime = 0.0f;
    float m_PlaybackSpeed = 1.0f;
    bool m_Loop = true;
    bool m_Playing = true;
    bool m_Initialized = false;
  };

}  // namespace Rain
