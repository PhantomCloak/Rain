#include "OzzAnimator.h"

#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>

#include "core/Log.h"

namespace Rain
{
  namespace
  {
    glm::mat4 OzzToGlm(const ozz::math::Float4x4& m)
    {
      glm::mat4 result;
      ozz::math::StorePtrU(m.cols[0], &result[0][0]);
      ozz::math::StorePtrU(m.cols[1], &result[1][0]);
      ozz::math::StorePtrU(m.cols[2], &result[2][0]);
      ozz::math::StorePtrU(m.cols[3], &result[3][0]);
      return result;
    }
  }  // namespace

  bool OzzAnimator::Initialize(Ref<OzzSkeleton> skeleton)
  {
    if (!skeleton || !skeleton->GetOzzSkeleton())
    {
      RN_LOG_ERR("OzzAnimator::Initialize - Invalid skeleton");
      return false;
    }

    m_Skeleton = skeleton;

    const auto* ozzSkeleton = skeleton->GetOzzSkeleton();
    int numJoints = ozzSkeleton->num_joints();
    int numSoaJoints = ozzSkeleton->num_soa_joints();

    m_SamplingContext.Resize(numJoints);

    m_LocalTransforms.resize(numSoaJoints);
    m_ModelMatrices.resize(numJoints);
    m_BoneMatrices.resize(numJoints, glm::mat4(1.0f));

    m_Initialized = true;
    RN_LOG("OzzAnimator initialized with {} joints", numJoints);

    return true;
  }

  void OzzAnimator::SetAnimation(Ref<OzzAnimation> animation)
  {
    m_Animation = animation;
    m_CurrentTime = 0.0f;
    m_SamplingContext.Invalidate();
  }

  void OzzAnimator::Update(float deltaTime)
  {
    if (!m_Initialized || !m_Playing || !m_Animation)
    {
      return;
    }

    const auto* ozzAnimation = m_Animation->GetOzzAnimation();
    if (!ozzAnimation)
    {
      return;
    }

    float duration = ozzAnimation->duration();
    if (duration <= 0.0f)
    {
      return;
    }

    m_CurrentTime += deltaTime * m_PlaybackSpeed;

    if (m_Loop)
    {
      while (m_CurrentTime > duration)
      {
        m_CurrentTime -= duration;
      }
      while (m_CurrentTime < 0.0f)
      {
        m_CurrentTime += duration;
      }
    }
    else
    {
      if (m_CurrentTime >= duration)
      {
        m_CurrentTime = duration;
        m_Playing = false;
      }
      else if (m_CurrentTime < 0.0f)
      {
        m_CurrentTime = 0.0f;
        m_Playing = false;
      }
    }

    ComputeBoneMatrices();
  }

  void OzzAnimator::Stop()
  {
    m_Playing = false;
    m_CurrentTime = 0.0f;
    m_SamplingContext.Invalidate();
  }

  void OzzAnimator::SetCurrentTime(float time)
  {
    m_CurrentTime = time;
    if (m_Animation)
    {
      float duration = m_Animation->GetDuration();
      if (duration > 0.0f)
      {
        m_CurrentTime = glm::clamp(m_CurrentTime, 0.0f, duration);
      }
    }
  }

  float OzzAnimator::GetNormalizedTime() const
  {
    if (!m_Animation)
    {
      return 0.0f;
    }
    float duration = m_Animation->GetDuration();
    if (duration <= 0.0f)
    {
      return 0.0f;
    }
    return m_CurrentTime / duration;
  }

  void OzzAnimator::ComputeBoneMatrices()
  {
    if (!m_Initialized || !m_Skeleton || !m_Animation)
    {
      return;
    }

    const auto* ozzSkeleton = m_Skeleton->GetOzzSkeleton();
    const auto* ozzAnimation = m_Animation->GetOzzAnimation();

    if (!ozzSkeleton || !ozzAnimation)
    {
      return;
    }

    float duration = ozzAnimation->duration();
    float ratio = (duration > 0.0f) ? (m_CurrentTime / duration) : 0.0f;
    ratio = glm::clamp(ratio, 0.0f, 1.0f);

    ozz::animation::SamplingJob samplingJob;
    samplingJob.animation = ozzAnimation;
    samplingJob.context = &m_SamplingContext;
    samplingJob.ratio = ratio;
    samplingJob.output = ozz::make_span(m_LocalTransforms);

    if (!samplingJob.Run())
    {
      RN_LOG_ERR("OzzAnimator: SamplingJob failed");
      return;
    }

    ozz::animation::LocalToModelJob localToModelJob;
    localToModelJob.skeleton = ozzSkeleton;
    localToModelJob.input = ozz::make_span(m_LocalTransforms);
    localToModelJob.output = ozz::make_span(m_ModelMatrices);

    if (!localToModelJob.Run())
    {
      RN_LOG_ERR("OzzAnimator: LocalToModelJob failed");
      return;
    }

    const auto& inverseBindMatrices = m_Skeleton->GetInverseBindMatrices();
    int numJoints = ozzSkeleton->num_joints();

    for (int i = 0; i < numJoints; ++i)
    {
      glm::mat4 modelMatrix = OzzToGlm(m_ModelMatrices[i]);

      if (i < static_cast<int>(inverseBindMatrices.size()))
      {
        m_BoneMatrices[i] = modelMatrix * inverseBindMatrices[i];
      }
      else
      {
        m_BoneMatrices[i] = modelMatrix;
      }
    }
  }

}  // namespace Rain
