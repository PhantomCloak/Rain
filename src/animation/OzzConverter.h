#pragma once

#include <assimp/scene.h>

#include <ozz/animation/offline/raw_skeleton.h>

#include "OzzAnimation.h"
#include "OzzSkeleton.h"
#include "Skeleton.h"
#include "core/Ref.h"

namespace Rain
{
  class OzzConverter
  {
   public:
    static Ref<OzzSkeleton> ConvertSkeleton(const aiScene* scene, const Skeleton& rainSkeleton);
    static Ref<OzzAnimation> ConvertAnimation(const aiAnimation* animation, const OzzSkeleton& ozzSkeleton);

   private:
    static void BuildRawSkeletonRecursive(
        const aiNode* node,
        const Skeleton& rainSkeleton,
        ozz::animation::offline::RawSkeleton::Joint& parentJoint,
        bool isRoot);

    static bool IsNodeABone(const aiNode* node, const Skeleton& rainSkeleton);
    static aiNode* FindBoneNode(const aiNode* root, const std::string& boneName);
  };

}  // namespace Rain
