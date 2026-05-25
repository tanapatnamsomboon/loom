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

        // Post-process tonemap pass — samples the linear HDR color texture
        // (typically the RGBA16F scene framebuffer's Color 0 attachment),
        // applies ACES filmic tonemap + sRGB encode, and writes the result
        // into the currently-bound framebuffer. If BloomPass was called this
        // frame, the bloom result is composited additively before tonemapping.
        // The caller is responsible for binding the LDR target framebuffer
        // + clearing it before this call.
        static void Tonemap(uint32_t hdr_color_texture_id);

        // Bloom pass — runs a Jimenez 2014 dual-filter chain (downsample +
        // upsample) on the HDR scene texture, extracting brights above
        // threshold and spreading them into a wide soft glow. The result is
        // cached internally and consumed by the next Tonemap() call. Lazy-
        // allocates / resizes the bloom mip chain when the scene size changes.
        // Pass scene_width / scene_height in pixels.
        static void BloomPass(uint32_t hdr_color_texture_id,
                              uint32_t scene_width, uint32_t scene_height);
        // Bloom controls (driven by Scene Properties UI; defaults applied at Init).
        static void SetBloomEnabled(bool enabled);
        static void SetBloomThreshold(float threshold);
        static void SetBloomIntensity(float intensity);

        // FXAA post-process pass — samples the tonemapped LDR/sRGB source and
        // writes anti-aliased output to the currently-bound framebuffer. Must
        // run AFTER tonemap (luma thresholds are tuned for sRGB display-space
        // input; running on linear HDR would over-smooth low-contrast regions).
        static void FXAAPass(uint32_t source_color_texture,
                             uint32_t width, uint32_t height);
        // FXAA enable toggle. Defaults to true. Set false for pixel-art 2D
        // scenes where the edge-direction heuristic destabilizes on the
        // perfectly-aligned pixel grid and produces noisy per-pixel blends.
        // ViewportPanel reads this to decide whether to run the FXAA pass.
        static void SetFXAAEnabled(bool enabled);
        static bool IsFXAAEnabled();

        // One draw call per submission (no batching). Any of the four texture
        // slots (albedo / ORM / emissive / normal) may be null — a 1×1 white
        // texture is used for albedo/ORM/emissive, and a 1×1 flat-normal texture
        // (128,128,255) is used for the normal map so no shader branch is needed.
        // ORM packs R=ambient occlusion, G=roughness, B=metallic (industry-standard
        // glTF convention). Emissive samples sRGB × emissive_factor (linear, HDR).
        // entity_id < 0 leaves the picking attachment untouched for this draw.
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
