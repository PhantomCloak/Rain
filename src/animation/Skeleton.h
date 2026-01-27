#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace Rain
{
  struct Bone
  {
    std::string Name;
    int32_t ParentIndex = -1;
    glm::mat4 InverseBindMatrix = glm::mat4(1.0f);
    glm::mat4 LocalTransform = glm::mat4(1.0f);  // Transform relative to parent
    glm::mat4 WorldTransform = glm::mat4(1.0f);  // Computed world transform
  };

  struct Skeleton
  {
    std::vector<Bone> Bones;
    std::unordered_map<std::string, uint32_t> BoneNameToIndex;
    std::vector<glm::mat4> BoneMatrices;  // Final matrices for GPU: WorldTransform * InverseBindMatrix
    static constexpr uint32_t MAX_BONES = 128;

    int32_t GetBoneIndex(const std::string& name) const
    {
      auto it = BoneNameToIndex.find(name);
      return it != BoneNameToIndex.end() ? static_cast<int32_t>(it->second) : -1;
    }

    // Sort bones so parents always come before children (topological sort)
    void SortBones()
    {
      if (Bones.empty())
      {
        return;
      }

      std::vector<Bone> sortedBones;
      std::vector<bool> processed(Bones.size(), false);
      std::unordered_map<std::string, uint32_t> newIndices;

      // Process bones - roots first, then children
      while (sortedBones.size() < Bones.size())
      {
        for (size_t i = 0; i < Bones.size(); i++)
        {
          if (processed[i])
          {
            continue;
          }

          // Root bone or parent already processed
          if (Bones[i].ParentIndex < 0 || processed[Bones[i].ParentIndex])
          {
            Bone bone = Bones[i];
            uint32_t newIndex = static_cast<uint32_t>(sortedBones.size());
            newIndices[bone.Name] = newIndex;

            // Update parent index to new index
            if (bone.ParentIndex >= 0)
            {
              bone.ParentIndex = static_cast<int32_t>(newIndices[Bones[bone.ParentIndex].Name]);
            }

            sortedBones.push_back(bone);
            processed[i] = true;
          }
        }
      }

      Bones = std::move(sortedBones);

      // Rebuild name-to-index map
      BoneNameToIndex.clear();
      for (size_t i = 0; i < Bones.size(); i++)
      {
        BoneNameToIndex[Bones[i].Name] = static_cast<uint32_t>(i);
      }
    }

    void ComputeBoneMatrices()
    {
      BoneMatrices.resize(Bones.size());
      for (size_t i = 0; i < Bones.size(); i++)
      {
        // Compute world transform from hierarchy
        if (Bones[i].ParentIndex >= 0)
        {
          Bones[i].WorldTransform = Bones[Bones[i].ParentIndex].WorldTransform * Bones[i].LocalTransform;
        }
        else
        {
          Bones[i].WorldTransform = Bones[i].LocalTransform;
        }
        // Final matrix = WorldTransform * InverseBindMatrix
        BoneMatrices[i] = Bones[i].WorldTransform * Bones[i].InverseBindMatrix;
      }
    }
  };

}  // namespace Rain
