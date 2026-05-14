#pragma once

#include "loom/core/core.h"
#include "loom/renderer/camera.h"
#include "loom/renderer/editor_camera.h"
#include "loom/renderer/mesh_asset.h"
#include "loom/renderer/texture.h"
#include <glm/glm.hpp>
#include <memory>

namespace Loom {

    class LOOM_API Renderer3D {
    public:
        static constexpr int kMaxDirectionalLights = 4;
        static constexpr int kMaxPointLights       = 16;

        struct DirectionalLight {
            glm::vec3 Direction; // world, normalized — direction the light propagates
            glm::vec3 Color;     // intensity baked in
        };

        struct PointLight {
            glm::vec3 Position; // world
            glm::vec3 Color;    // intensity baked in
            float     Range;    // contribution falls to zero past this distance
        };

        static void Init();
        static void Shutdown();

        static void BeginScene(const EditorCamera& camera);
        static void BeginScene(const Camera& camera, const glm::mat4& transform);
        static void EndScene();

        // Optional — if not called, the scene renders with only the fallback ambient.
        // Lights beyond the kMax* limits are silently dropped.
        static void SetLights(const DirectionalLight* dir_lights, int dir_count,
                              const PointLight*       point_lights, int point_count);

        // One draw call per submission (no batching). albedo_texture may be null
        // (a 1x1 white texture is bound in its place). entity_id < 0 leaves the
        // picking attachment untouched for this draw.
        static void Submit(const std::shared_ptr<MeshAsset>& mesh,
                           const glm::vec4& albedo_color,
                           const std::shared_ptr<Texture2D>& albedo_texture,
                           const glm::mat4& transform,
                           float roughness = 0.5f,
                           float metallic  = 0.0f,
                           int   entity_id = -1);
    };

} // namespace Loom
