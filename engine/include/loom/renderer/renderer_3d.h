#pragma once

#include "loom/core/core.h"
#include "loom/renderer/camera.h"
#include "loom/renderer/cubemap.h"
#include "loom/renderer/editor_camera.h"
#include "loom/renderer/mesh_asset.h"
#include "loom/renderer/texture.h"
#include <glm/glm.hpp>
#include <memory>

namespace Loom {

    class LOOM_API Renderer3D {
    public:
        static constexpr int      kMaxDirectionalLights = 4;
        static constexpr int      kMaxPointLights       = 16;
        // Square shadow map resolution per cascade. Higher = sharper shadows + more VRAM.
        // 2048 × 4 cascades × DEPTH32F = ~64 MB of shadow VRAM.
        static constexpr uint32_t kShadowMapSize        = 2048;
        // Cascaded shadow maps — N depth slices of the camera frustum, each
        // with its own shadow map at full resolution. Indexes the per-cascade
        // arrays below. mesh.frag has hand-unrolled branches matching this N;
        // changing the count requires editing the shader too.
        static constexpr int      kCascadeCount         = 4;

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

        // IBL environment. Pass null to disable IBL for the next draws.
        //   * `SetIrradianceMap` — diffuse ambient term (Lambertian-convolved
        //     env cubemap, B.2).
        //   * `SetPrefilterMap` — specular IBL via the Karis split-sum
        //     approximation (B.3). Cubemap is roughness-convolved per mip;
        //     the shader samples `textureLod(prefilter, R, roughness * maxLOD)`.
        //     The BRDF LUT half of the split-sum is owned by Renderer3D
        //     internally (generated once at Init).
        static void SetIrradianceMap(const std::shared_ptr<TextureCubemap>& irradiance);
        static void SetPrefilterMap(const std::shared_ptr<TextureCubemap>& prefilter);

        // Renders `cubemap` as a skybox using the currently-bound framebuffer
        // and viewport. `view` is the camera view matrix (translation is zeroed
        // internally so the skybox is camera-centered); `projection` is the
        // camera projection. No-op when `cubemap` is null. Cubemap binds to
        // texture unit 0 for the duration of the draw.
        static void DrawSkybox(const glm::mat4& view, const glm::mat4& projection,
                               const std::shared_ptr<TextureCubemap>& cubemap);

        // Debug visualization mode for the mesh shader (see mesh.frag uDebugViz).
        //   0=PBR (default), 1=irradiance, 2=normal, 3=NdotL, 4=NdotV, 5=albedo.
        static void SetDebugViz(int mode);

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

        // ── Shadow pass (cascaded) ─────────────────────────────────────────
        // Caller workflow per frame (only when a shadow-casting directional light exists):
        //   Renderer3D::SetCascadeSplits(splits);  // world-space far distance per cascade
        //   for (int i = 0; i < kCascadeCount; ++i) {
        //       Renderer3D::BeginShadowPass(i, light_vp_for_cascade);
        //       for each mesh entity: Renderer3D::SubmitShadow(mesh, world);
        //       Renderer3D::EndShadowPass();
        //   }
        //   ... then the regular BeginScene / Submit / EndScene path runs as before
        //
        // Subsequent Submit() calls automatically sample the appropriate cascade
        // per fragment (based on view-space depth) and attenuate the first
        // directional light by its visibility. Skipping the shadow pass entirely
        // is fine — Submit falls back to no shadows.
        static void SetCascadeSplits(const float splits[kCascadeCount]); // world distances along view direction
        static void BeginShadowPass(int cascade_index, const glm::mat4& light_view_projection);
        static void SubmitShadow(const std::shared_ptr<MeshAsset>& mesh,
                                 const glm::mat4& transform);
        static void EndShadowPass();
    };

} // namespace Loom
