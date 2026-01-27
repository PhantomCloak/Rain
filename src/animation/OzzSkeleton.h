#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/memory/unique_ptr.h>

#include "core/Ref.h"

namespace Rain
{
  class OzzSkeleton
  {
   public:
    OzzSkeleton() = default;
    ~OzzSkeleton() = default;

    OzzSkeleton(const OzzSkeleton&) = delete;
    OzzSkeleton& operator=(const OzzSkeleton&) = delete;
    OzzSkeleton(OzzSkeleton&&) = default;
    OzzSkeleton& operator=(OzzSkeleton&&) = default;

    void SetSkeleton(ozz::unique_ptr<ozz::animation::Skeleton> skeleton);

    const ozz::animation::Skeleton* GetOzzSkeleton() const { return m_Skeleton.get(); }
    ozz::animation::Skeleton* GetOzzSkeleton() { return m_Skeleton.get(); }

    int GetNumJoints() const { return m_Skeleton ? m_Skeleton->num_joints() : 0; }
    int GetNumSoaJoints() const { return m_Skeleton ? m_Skeleton->num_soa_joints() : 0; }

    const std::vector<glm::mat4>& GetInverseBindMatrices() const { return m_InverseBindMatrices; }
    void SetInverseBindMatrices(std::vector<glm::mat4>&& matrices) { m_InverseBindMatrices = std::move(matrices); }

    int32_t GetJointIndex(const std::string& name) const;
    const char* GetJointName(int index) const;

    void BuildNameToIndexMap();

   private:
    ozz::unique_ptr<ozz::animation::Skeleton> m_Skeleton;
    std::vector<glm::mat4> m_InverseBindMatrices;
    std::unordered_map<std::string, int32_t> m_JointNameToIndex;
  };

}  // namespace Rain
