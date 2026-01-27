#include "OzzSkeleton.h"

namespace Rain
{
  void OzzSkeleton::SetSkeleton(ozz::unique_ptr<ozz::animation::Skeleton> skeleton)
  {
    m_Skeleton = std::move(skeleton);
    BuildNameToIndexMap();
  }

  int32_t OzzSkeleton::GetJointIndex(const std::string& name) const
  {
    auto it = m_JointNameToIndex.find(name);
    return it != m_JointNameToIndex.end() ? it->second : -1;
  }

  const char* OzzSkeleton::GetJointName(int index) const
  {
    if (!m_Skeleton || index < 0 || index >= m_Skeleton->num_joints())
    {
      return nullptr;
    }
    return m_Skeleton->joint_names()[index];
  }

  void OzzSkeleton::BuildNameToIndexMap()
  {
    m_JointNameToIndex.clear();
    if (!m_Skeleton)
    {
      return;
    }

    auto names = m_Skeleton->joint_names();
    for (int i = 0; i < m_Skeleton->num_joints(); ++i)
    {
      m_JointNameToIndex[names[i]] = i;
    }
  }

}  // namespace Rain
