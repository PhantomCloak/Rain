/**
 * @file OzzConverter.cpp
 * @brief Converts Assimp skeletal data to Ozz-animation runtime format.
 *
 * ## Overview
 * This converter bridges two libraries:
 * - **Assimp**: Import library that loads 3D models (glTF, FBX, etc.) into a scene graph
 * - **Ozz-animation**: Runtime animation library optimized for performance (SIMD, cache-friendly)
 *
 * ## Why We Need This Converter
 * Assimp gives us raw imported data in its own format (aiNode, aiAnimation, aiBone).
 * Ozz-animation has its own optimized runtime structures (ozz::animation::Skeleton, Animation).
 * This converter transforms Assimp's import-time data into Ozz's runtime-optimized format.
 *
 * ## Key Concepts
 *
 * ### Skeleton vs Bones
 * - **Bones** (in Assimp): Defined per-mesh, each bone has vertices it influences
 * - **Skeleton** (in Ozz): A hierarchy of joints that can be animated as a unit
 * - We extract bone info from meshes but build the hierarchy from the scene graph (aiNode tree)
 *
 * ### The Conversion Pipeline
 * 1. Rain's Mesh loader extracts bone data from Assimp meshes → creates Rain::Skeleton
 * 2. OzzConverter::ConvertSkeleton() builds Ozz skeleton from Rain::Skeleton + aiScene
 * 3. OzzConverter::ConvertAnimation() maps Assimp animation channels to Ozz tracks
 *
 * ### Inverse Bind Matrices
 * These matrices transform vertices from model space to bone space. They're computed
 * at import time and stored with each bone. We preserve these when converting to Ozz
 * so skinning calculations work correctly.
 */

#include "OzzConverter.h"

#include <glm/gtc/type_ptr.hpp>
#include <queue>
#include <unordered_set>

// Ozz offline tools - used for building runtime structures from raw data
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/base/maths/quaternion.h>
#include <ozz/base/maths/transform.h>
#include <ozz/base/maths/vec_float.h>

#include "core/Log.h"

namespace Rain
{
  namespace
  {
    /**
     * @brief Converts Assimp's row-major 4x4 matrix to GLM's column-major format.
     *
     * Assimp uses row-major matrices (aiMatrix4x4), while GLM uses column-major (glm::mat4).
     * The indices map as follows:
     *   Assimp: [row][col] -> a1=row0col0, b1=row1col0, etc.
     *   GLM:    [col][row] -> [0][0]=col0row0, [0][1]=col0row1, etc.
     *
     * Note: This function is currently unused but kept for potential future use
     * when we need full matrix conversion (e.g., for computing world transforms).
     */
    glm::mat4 ConvertAssimpMatrix(const aiMatrix4x4& from)
    {
      glm::mat4 to;
      to[0][0] = from.a1;
      to[0][1] = from.b1;
      to[0][2] = from.c1;
      to[0][3] = from.d1;
      to[1][0] = from.a2;
      to[1][1] = from.b2;
      to[1][2] = from.c2;
      to[1][3] = from.d2;
      to[2][0] = from.a3;
      to[2][1] = from.b3;
      to[2][2] = from.c3;
      to[2][3] = from.d3;
      to[3][0] = from.a4;
      to[3][1] = from.b4;
      to[3][2] = from.c4;
      to[3][3] = from.d4;
      return to;
    }

    /**
     * @brief Decomposes a 4x4 transformation matrix into translation, rotation, scale.
     *
     * Ozz stores joint transforms as separate T/R/S components rather than matrices.
     * This is more efficient for animation blending and interpolation:
     * - Translations can be linearly interpolated
     * - Rotations use quaternion slerp
     * - Scales can be linearly interpolated
     *
     * Combining these after interpolation is cheaper than matrix interpolation.
     *
     * @param matrix The Assimp 4x4 transformation matrix (typically a node's local transform)
     * @return ozz::math::Transform with separate translation, rotation, scale
     */
    ozz::math::Transform DecomposeTransform(const aiMatrix4x4& matrix)
    {
      aiVector3D scaling, position;
      aiQuaternion rotation;
      matrix.Decompose(scaling, rotation, position);

      ozz::math::Transform transform;
      transform.translation = ozz::math::Float3(position.x, position.y, position.z);
      transform.rotation = ozz::math::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
      transform.scale = ozz::math::Float3(scaling.x, scaling.y, scaling.z);

      return transform;
    }
  }  // namespace

  /**
   * @brief Checks if a scene graph node corresponds to a bone in our skeleton.
   *
   * The Assimp scene graph contains ALL nodes - meshes, cameras, lights, bones, etc.
   * We only want to include nodes that are actual bones. We determine this by checking
   * if the node's name exists in our Rain::Skeleton's bone name map.
   *
   * @param node The Assimp scene graph node to check
   * @param rainSkeleton Our skeleton data extracted during mesh loading
   * @return true if this node is a bone we care about
   */
  bool OzzConverter::IsNodeABone(const aiNode* node, const Skeleton& rainSkeleton)
  {
    if (!node)
    {
      return false;
    }
    std::string nodeName(node->mName.C_Str());
    return rainSkeleton.BoneNameToIndex.find(nodeName) != rainSkeleton.BoneNameToIndex.end();
  }

  /**
   * @brief Recursively searches the scene graph for a node with a specific name.
   *
   * We need this because bone data comes from two places in Assimp:
   * 1. aiBone structures (attached to meshes) - give us bone names and inverse bind matrices
   * 2. aiNode tree (scene graph) - gives us the actual hierarchy and local transforms
   *
   * This function finds the scene graph node for a given bone name so we can
   * traverse its children and build the skeleton hierarchy correctly.
   *
   * @param root The root node to start searching from
   * @param boneName The name of the bone/joint to find
   * @return Pointer to the node, or nullptr if not found
   */
  aiNode* OzzConverter::FindBoneNode(const aiNode* root, const std::string& boneName)
  {
    if (!root)
    {
      return nullptr;
    }

    if (std::string(root->mName.C_Str()) == boneName)
    {
      return const_cast<aiNode*>(root);
    }

    for (unsigned int i = 0; i < root->mNumChildren; ++i)
    {
      aiNode* found = FindBoneNode(root->mChildren[i], boneName);
      if (found)
      {
        return found;
      }
    }

    return nullptr;
  }

  /**
   * @brief Recursively builds Ozz's RawSkeleton joint hierarchy from Assimp nodes.
   *
   * ## How It Works
   * This function walks the Assimp scene graph and builds a parallel Ozz joint tree,
   * but ONLY including nodes that are actual bones (skipping meshes, cameras, etc.).
   *
   * ## The "Skip Non-Bone Nodes" Logic
   * If a node is NOT a bone, we still recurse into its children. This handles cases like:
   *   Armature (not a bone)
   *     └── Hips (bone - root)
   *           ├── Spine (bone)
   *           └── LeftLeg (bone)
   *
   * We skip "Armature" but still find "Hips" and make it the skeleton root.
   *
   * ## Why We Build a Tree (Not a Flat List)
   * Ozz's RawSkeleton uses a nested structure where each Joint has a `children` vector.
   * This matches how skeletal animation works - child bones inherit parent transforms.
   * The SkeletonBuilder later flattens this into an optimized runtime format.
   *
   * @param node Current Assimp node being processed
   * @param rainSkeleton Our skeleton data for bone name lookups
   * @param parentJoint The parent Ozz joint to add children to
   * @param isRoot True if this is the skeleton root (handled specially)
   */
  void OzzConverter::BuildRawSkeletonRecursive(
      const aiNode* node,
      const Skeleton& rainSkeleton,
      ozz::animation::offline::RawSkeleton::Joint& parentJoint,
      bool isRoot)
  {
    if (!node)
    {
      return;
    }

    std::string nodeName(node->mName.C_Str());
    bool isBone = IsNodeABone(node, rainSkeleton);

    if (isBone)
    {
      // This node is a bone - create a joint for it
      ozz::animation::offline::RawSkeleton::Joint joint;
      joint.name = nodeName;
      // Store the node's LOCAL transform (relative to parent)
      // Ozz will compute world transforms at runtime by chaining these
      joint.transform = DecomposeTransform(node->mTransformation);

      // Recurse into children, adding any child bones to THIS joint
      for (unsigned int i = 0; i < node->mNumChildren; ++i)
      {
        BuildRawSkeletonRecursive(node->mChildren[i], rainSkeleton, joint, false);
      }

      if (isRoot)
      {
        // Special case: root joint replaces the placeholder passed in
        parentJoint = std::move(joint);
      }
      else
      {
        // Normal case: add as child of parent
        parentJoint.children.push_back(std::move(joint));
      }
    }
    else
    {
      // This node is NOT a bone (e.g., "Armature" container node)
      // Skip it but recurse into children - there may be bones below
      for (unsigned int i = 0; i < node->mNumChildren; ++i)
      {
        BuildRawSkeletonRecursive(node->mChildren[i], rainSkeleton, parentJoint, isRoot);
      }
    }
  }

  /**
   * @brief Converts Rain::Skeleton (from Assimp) to an Ozz runtime skeleton.
   *
   * ## The Conversion Process
   *
   * ### Step 1: Build RawSkeleton (Offline Data Structure)
   * Ozz uses a two-phase approach:
   * - RawSkeleton: Human-readable, editable, hierarchical (used here)
   * - Skeleton: Optimized runtime format (flat arrays, SIMD-friendly)
   *
   * ### Step 2: Find the Root Bone
   * We find the bone with ParentIndex < 0 (no parent = root). Then we locate
   * its corresponding node in the Assimp scene graph to get hierarchy info.
   *
   * ### Step 3: Build Joint Hierarchy
   * Starting from the root, we recursively build the joint tree, capturing
   * each joint's local transform from its scene graph node.
   *
   * ### Step 4: Build Runtime Skeleton
   * SkeletonBuilder converts RawSkeleton to the optimized runtime format.
   * This flattens the tree into arrays ordered for efficient traversal.
   *
   * ### Step 5: Transfer Inverse Bind Matrices
   * CRITICAL: Ozz's skeleton doesn't store inverse bind matrices (it's a pure
   * animation library). But we need them for skinning! So we store them in
   * OzzSkeleton separately, remapped to match Ozz's joint ordering.
   *
   * ## Why Joint Order Matters
   * Ozz may reorder joints for cache efficiency. Our Rain::Skeleton has one
   * ordering, Ozz has another. We must remap inverse bind matrices to Ozz's
   * order, which is why we look up joints by name.
   *
   * @param scene The Assimp scene (needed for node hierarchy)
   * @param rainSkeleton Our skeleton data (bone names, inverse bind matrices)
   * @return Ref<OzzSkeleton> ready for animation, or nullptr on failure
   */
  Ref<OzzSkeleton> OzzConverter::ConvertSkeleton(const aiScene* scene, const Skeleton& rainSkeleton)
  {
    if (!scene || rainSkeleton.Bones.empty())
    {
      RN_LOG_ERR("OzzConverter::ConvertSkeleton - Invalid input");
      return nullptr;
    }

    ozz::animation::offline::RawSkeleton rawSkeleton;

    // Step 1: Find the root bone (the one with no parent)
    // In a valid skeleton, exactly one bone should have ParentIndex < 0
    aiNode* rootBoneNode = nullptr;
    for (const auto& bone : rainSkeleton.Bones)
    {
      if (bone.ParentIndex < 0)
      {
        rootBoneNode = FindBoneNode(scene->mRootNode, bone.Name);
        break;
      }
    }

    if (!rootBoneNode)
    {
      RN_LOG_ERR("OzzConverter::ConvertSkeleton - Could not find root bone node");
      return nullptr;
    }

    // Step 2: Build the joint hierarchy starting from root
    ozz::animation::offline::RawSkeleton::Joint rootJoint;
    rootJoint.name = rootBoneNode->mName.C_Str();
    rootJoint.transform = DecomposeTransform(rootBoneNode->mTransformation);

    // Recursively add all child bones
    for (unsigned int i = 0; i < rootBoneNode->mNumChildren; ++i)
    {
      BuildRawSkeletonRecursive(rootBoneNode->mChildren[i], rainSkeleton, rootJoint, false);
    }

    // RawSkeleton supports multiple roots (for multi-root skeletons), but we have one
    rawSkeleton.roots.push_back(std::move(rootJoint));

    // Ozz validates: no duplicate names, valid transforms, proper hierarchy
    if (!rawSkeleton.Validate())
    {
      RN_LOG_ERR("OzzConverter::ConvertSkeleton - RawSkeleton validation failed");
      return nullptr;
    }

    RN_LOG("OzzConverter: RawSkeleton has {} joints", rawSkeleton.num_joints());

    // Step 3: Build optimized runtime skeleton
    // This converts the tree to flat arrays with parent indices
    ozz::animation::offline::SkeletonBuilder builder;
    auto ozzSkeletonPtr = builder(rawSkeleton);

    if (!ozzSkeletonPtr)
    {
      RN_LOG_ERR("OzzConverter::ConvertSkeleton - SkeletonBuilder failed");
      return nullptr;
    }

    auto result = CreateRef<OzzSkeleton>();
    result->SetSkeleton(std::move(ozzSkeletonPtr));

    // Step 4: Remap inverse bind matrices to Ozz joint order
    // Ozz joints may be in a different order than Rain's bones!
    // We look up each Ozz joint by name to find the corresponding Rain bone.
    int numOzzJoints = result->GetNumJoints();
    std::vector<glm::mat4> inverseBindMatrices(numOzzJoints, glm::mat4(1.0f));

    for (int i = 0; i < numOzzJoints; ++i)
    {
      const char* jointName = result->GetJointName(i);
      if (jointName)
      {
        auto it = rainSkeleton.BoneNameToIndex.find(jointName);
        if (it != rainSkeleton.BoneNameToIndex.end())
        {
          uint32_t rainIndex = it->second;
          if (rainIndex < rainSkeleton.Bones.size())
          {
            // Copy inverse bind matrix from Rain bone to Ozz joint slot
            inverseBindMatrices[i] = rainSkeleton.Bones[rainIndex].InverseBindMatrix;
          }
        }
      }
    }

    result->SetInverseBindMatrices(std::move(inverseBindMatrices));

    RN_LOG("OzzConverter: Created OzzSkeleton with {} joints", numOzzJoints);

    return result;
  }

  Ref<OzzAnimation> OzzConverter::ConvertAnimation(const aiAnimation* animation, const OzzSkeleton& ozzSkeleton)
  {
    if (!animation || !ozzSkeleton.GetOzzSkeleton())
    {
      RN_LOG_ERR("OzzConverter::ConvertAnimation - Invalid input");
      return nullptr;
    }

    const auto* skeleton = ozzSkeleton.GetOzzSkeleton();
    int numJoints = skeleton->num_joints();

    ozz::animation::offline::RawAnimation rawAnimation;
    rawAnimation.duration = static_cast<float>(animation->mDuration / animation->mTicksPerSecond);
    rawAnimation.name = animation->mName.C_Str();
    rawAnimation.tracks.resize(numJoints);

    for (unsigned int channelIdx = 0; channelIdx < animation->mNumChannels; ++channelIdx)
    {
      const aiNodeAnim* channel = animation->mChannels[channelIdx];
      std::string boneName(channel->mNodeName.C_Str());

      int jointIndex = ozzSkeleton.GetJointIndex(boneName);
      if (jointIndex < 0)
      {
        continue;
      }

      auto& track = rawAnimation.tracks[jointIndex];

      for (unsigned int keyIdx = 0; keyIdx < channel->mNumPositionKeys; ++keyIdx)
      {
        const aiVectorKey& key = channel->mPositionKeys[keyIdx];
        float time = static_cast<float>(key.mTime / animation->mTicksPerSecond);

        ozz::animation::offline::RawAnimation::TranslationKey translationKey;
        translationKey.time = time;
        translationKey.value = ozz::math::Float3(key.mValue.x, key.mValue.y, key.mValue.z);
        track.translations.push_back(translationKey);
      }

      for (unsigned int keyIdx = 0; keyIdx < channel->mNumRotationKeys; ++keyIdx)
      {
        const aiQuatKey& key = channel->mRotationKeys[keyIdx];
        float time = static_cast<float>(key.mTime / animation->mTicksPerSecond);

        ozz::animation::offline::RawAnimation::RotationKey rotationKey;
        rotationKey.time = time;
        rotationKey.value = ozz::math::Quaternion(key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w);
        track.rotations.push_back(rotationKey);
      }

      for (unsigned int keyIdx = 0; keyIdx < channel->mNumScalingKeys; ++keyIdx)
      {
        const aiVectorKey& key = channel->mScalingKeys[keyIdx];
        float time = static_cast<float>(key.mTime / animation->mTicksPerSecond);

        ozz::animation::offline::RawAnimation::ScaleKey scaleKey;
        scaleKey.time = time;
        scaleKey.value = ozz::math::Float3(key.mValue.x, key.mValue.y, key.mValue.z);
        track.scales.push_back(scaleKey);
      }
    }

    for (int i = 0; i < numJoints; ++i)
    {
      auto& track = rawAnimation.tracks[i];
      if (track.translations.empty())
      {
        ozz::animation::offline::RawAnimation::TranslationKey key;
        key.time = 0.0f;
        key.value = ozz::math::Float3::zero();
        track.translations.push_back(key);
      }
      if (track.rotations.empty())
      {
        ozz::animation::offline::RawAnimation::RotationKey key;
        key.time = 0.0f;
        key.value = ozz::math::Quaternion::identity();
        track.rotations.push_back(key);
      }
      if (track.scales.empty())
      {
        ozz::animation::offline::RawAnimation::ScaleKey key;
        key.time = 0.0f;
        key.value = ozz::math::Float3::one();
        track.scales.push_back(key);
      }
    }

    if (!rawAnimation.Validate())
    {
      RN_LOG_ERR("OzzConverter::ConvertAnimation - RawAnimation validation failed");
      return nullptr;
    }

    ozz::animation::offline::AnimationBuilder builder;
    auto ozzAnimationPtr = builder(rawAnimation);

    if (!ozzAnimationPtr)
    {
      RN_LOG_ERR("OzzConverter::ConvertAnimation - AnimationBuilder failed");
      return nullptr;
    }

    auto result = CreateRef<OzzAnimation>();
    result->SetAnimation(std::move(ozzAnimationPtr));

    RN_LOG("OzzConverter: Created OzzAnimation '{}' with duration {} seconds",
           result->GetName(), result->GetDuration());

    return result;
  }

}  // namespace Rain
