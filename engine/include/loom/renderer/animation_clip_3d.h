#pragma once

#include "loom/core/core.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

namespace Loom {

    // glTF animation sampler interpolation modes. Mirrors cgltf_interpolation_type
    // so we can pass it across the import boundary unchanged.
    enum class Interpolation { Linear, Step, CubicSpline };

    // One TRS channel for one joint. Either ValuesVec3 (for T/S) or ValuesQuat
    // (for R) is populated, never both. Times has the same length as the
    // populated values vector for Linear/Step; for CubicSpline the value vector
    // is 3x as long (tangent_in, value, tangent_out per keyframe). Empty
    // Times means "this joint doesn't animate this channel; use bind pose".
    struct JointChannel {
        std::vector<float>      Times;      // seconds, monotonic
        std::vector<glm::vec3>  ValuesVec3; // populated for Translation / Scale
        std::vector<glm::quat>  ValuesQuat; // populated for Rotation
        Interpolation           Interp = Interpolation::Linear;

        bool Empty() const { return Times.empty(); }
    };

    // All animated channels for a single joint within one clip. JointIndex
    // refers to the owning Skeleton's joint array.
    struct JointAnimationTrack {
        int          JointIndex = -1;
        JointChannel Translation;
        JointChannel Rotation;
        JointChannel Scale;
    };

    // A single named clip. Owned by MeshAsset alongside the Skeleton; clip names
    // are unique within a MeshAsset (collisions get suffixed at import time).
    struct AnimationClip3D {
        std::string                      Name;
        float                            Duration = 0.0f; // seconds; max end-time across all sampler inputs
        std::vector<JointAnimationTrack> Tracks;
    };

} // namespace Loom
