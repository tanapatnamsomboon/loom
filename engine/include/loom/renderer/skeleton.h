#pragma once

#include "loom/core/core.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Loom {

    // Per-joint data for a skinned mesh. Indices are joint indices into the
    // Skeleton's parallel arrays (NOT cgltf node indices). Parent == -1 means
    // the joint has no parent inside the skeleton (its world transform is
    // taken as joint-local).
    struct SkeletonJoint {
        int         Parent = -1;
        glm::mat4   LocalBind   = glm::mat4(1.0f); // bind-pose local TRS (default for un-animated playback)
        glm::mat4   InverseBind = glm::mat4(1.0f); // glTF skin's inverseBindMatrices entry
        std::string Name;
    };

    // Skeleton data lives on MeshAsset (asset-shared, not per-entity). Holds
    // everything needed to walk the bind pose and compute skin matrices.
    // Per-entity animation state (current time, sampled TRS, etc.) lives on
    // an AnimationComponent in a later slice.
    //
    // Hard cap of 128 joints matches the Bones UBO size declared in
    // mesh_skinned.vert; assets exceeding it are clamped on load.
    struct Skeleton {
        static constexpr int kMaxJoints = 128;

        std::vector<SkeletonJoint> Joints;

        // World transform of the non-joint ancestor chain above the skin's
        // root joint(s). Captures the scene-root rotation that exporters
        // often apply (e.g., Z-up -> Y-up). Multiplied into joint_world for
        // joints with Parent == -1 during the per-frame skeleton walk.
        // Defaults to identity when the root joint sits directly under the
        // scene root with no transform.
        glm::mat4 RootWorld = glm::mat4(1.0f);

        int JointCount() const { return (int)Joints.size(); }
        bool Empty()      const { return Joints.empty(); }
    };

} // namespace Loom
