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
        static void Init();
        static void Shutdown();

        static void BeginScene(const EditorCamera& camera);
        static void BeginScene(const Camera& camera, const glm::mat4& transform);
        static void EndScene();

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
