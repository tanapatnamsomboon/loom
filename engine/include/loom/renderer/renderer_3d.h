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
        // 4096 x 4 cascades x DEPTH32F = ~256 MB shadow VRAM. Heavy, but
        // the editor camera tends to roam far enough that the distant
        // cascades show visible texel pixelation at 2048. Used as the
        // fall-back when no project has overridden GraphicsConfig.
        static constexpr uint32_t kDefaultShadowMapSize     = 4096;
        static constexpr float    kDefaultShadowMaxDistance = 200.0f;
        // Changing this requires editing mesh.frag (branches are hand-unrolled).
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

        // IBL environment cubemaps; pass null to disable. The BRDF LUT half of
        // the split-sum is owned by Renderer3D (generated once at Init).
        static void SetIrradianceMap(const std::shared_ptr<TextureCubemap>& irradiance);
        static void SetPrefilterMap(const std::shared_ptr<TextureCubemap>& prefilter);

        // Skybox. View translation is zeroed internally (camera-centered);
        // no-op when cubemap is null. Cubemap binds to unit 0.
        static void DrawSkybox(const glm::mat4& view, const glm::mat4& projection,
                               const std::shared_ptr<TextureCubemap>& cubemap);

        // Debug viz mode for mesh.frag uDebugViz.
        // 0=PBR, 1=irradiance, 2=normal, 3=NdotL, 4=NdotV, 5=albedo, 6=prefilter, 7=BRDF LUT.
        static void SetDebugViz(int mode);

        // Post-process tonemap. Samples linear HDR + (optional cached) bloom,
        // writes ACES + sRGB to the currently-bound framebuffer.
        static void Tonemap(uint32_t hdr_color_texture_id);

        // Jimenez 2014 dual-filter bloom on the HDR scene; result is cached for
        // the next Tonemap() call. Lazy-resizes the mip chain when size changes.
        static void BloomPass(uint32_t hdr_color_texture_id,
                              uint32_t scene_width, uint32_t scene_height);
        static void  SetBloomEnabled(bool enabled);
        static bool  IsBloomEnabled();
        static void  SetBloomThreshold(float threshold);
        static float GetBloomThreshold();
        static void  SetBloomIntensity(float intensity);
        static float GetBloomIntensity();

        // FXAA on the tonemapped sRGB source (must run AFTER tonemap —
        // luma thresholds are tuned for display space).
        static void FXAAPass(uint32_t source_color_texture,
                             uint32_t width, uint32_t height);
        // Disable for pixel-art 2D: edge-direction heuristic destabilizes on aligned grids.
        static void SetFXAAEnabled(bool enabled);
        static bool IsFXAAEnabled();

        // One draw call per submission. Any texture slot may be null (1x1 white
        // fallback; flat-normal fallback for normal_texture). ORM packs
        // R=AO, G=roughness, B=metallic. entity_id < 0 skips the picking write.
        //
        // sampled_local_transforms (skinned meshes only): when provided, the
        // skeleton walk uses sampled_local_transforms[i] instead of the
        // skeleton's bind-pose LocalBind for joint i. Pass null + count=0 to
        // render the bind pose. Indices past sampled_count fall back to bind.
        static void Submit(const std::shared_ptr<MeshAsset>& mesh,
                           const glm::vec4& albedo_color,
                           const std::shared_ptr<Texture2D>& albedo_texture,
                           const std::shared_ptr<Texture2D>& orm_texture,
                           const std::shared_ptr<Texture2D>& emissive_texture,
                           const glm::vec3& emissive_factor,
                           const std::shared_ptr<Texture2D>& normal_texture,
                           const glm::mat4& transform,
                           float roughness = 0.5f,
                           float metallic  = 0.0f,
                           int   entity_id = -1,
                           const glm::mat4* sampled_local_transforms = nullptr,
                           int   sampled_count = 0);

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

        // Project-driven quality knobs. Apply per project (typically right after
        // the project loads). SetShadowMapSize re-creates the cascade FBOs only
        // when the size actually changed. Get* returns the current live value.
        static void     SetShadowMapSize(uint32_t size);
        static uint32_t GetShadowMapSize();
        static void     SetShadowMaxDistance(float distance);
        static float    GetShadowMaxDistance();
        // sampled_local_transforms (skinned meshes only): same semantics as
        // Submit's parameter. Null + count=0 renders the bind-pose silhouette;
        // pass an animator's sampled bones to cast a correctly-shaped shadow.
        static void SubmitShadow(const std::shared_ptr<MeshAsset>& mesh,
                                 const glm::mat4& transform,
                                 const glm::mat4* sampled_local_transforms = nullptr,
                                 int   sampled_count = 0);
        static void EndShadowPass();
    };

} // namespace Loom
