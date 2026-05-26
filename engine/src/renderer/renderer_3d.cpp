#include "loom/renderer/renderer_3d.h"
#include "loom/asset/asset_manager.h"
#include "loom/core/log.h"
#include "loom/project/project.h"
#include "loom/renderer/buffer.h"
#include "loom/renderer/framebuffer.h"
#include "loom/renderer/render_command.h"
#include "loom/renderer/shader.h"
#include "loom/renderer/vertex_array.h"
#include <glad/glad.h>
#include <algorithm>

namespace Loom {

    struct Renderer3DStorage {
        std::shared_ptr<Shader>    MeshShader;
        std::shared_ptr<Shader>    MeshSkinnedShader; // used when MeshAsset::IsSkinned()
        std::shared_ptr<Shader>    ShadowShader;
        std::shared_ptr<Texture2D> WhiteTexture;
        // 1x1 (128,128,255,255) — decodes to tangent-space (0,0,1) so TBN
        // returns the geometry normal unchanged when no map is bound.
        std::shared_ptr<Texture2D> FlatNormalTexture;

        struct CameraData {
            glm::mat4 ViewProjection;
        };
        CameraData                     CameraBuffer;
        std::shared_ptr<UniformBuffer> CameraUniformBuffer;
        // Skin matrices UBO — binding=1, sized for Skeleton::kMaxJoints (128
        // mat4s = 8 KB). Updated only on skinned-mesh Submit calls.
        std::shared_ptr<UniformBuffer> BonesUniformBuffer;

        glm::vec3 ViewPosition = glm::vec3(0.0f);

        glm::vec3 DirLightDir[Renderer3D::kMaxDirectionalLights];
        glm::vec3 DirLightColor[Renderer3D::kMaxDirectionalLights];
        int       DirLightCount = 0;

        glm::vec3 PointLightPos[Renderer3D::kMaxPointLights];
        glm::vec3 PointLightColor[Renderer3D::kMaxPointLights];
        float     PointLightRange[Renderer3D::kMaxPointLights];
        int       PointLightCount = 0;

        // Mesh-shader texture unit layout:
        //   0    = albedo
        //   1-4  = shadow cascades
        //   5    = irradiance cubemap
        //   6    = prefiltered env (mips = roughness)
        //   7    = BRDF LUT (split-sum)
        //   8    = ORM (R=AO, G=rough, B=metal)
        //   9    = emissive
        //   10   = normal map
        std::shared_ptr<TextureCubemap> IrradianceMap;
        std::shared_ptr<TextureCubemap> PrefilterMap;
        GLuint                          BRDFLUT          = 0; // RG16F, raw GL (no engine abstraction needed)
        float                           MaxReflectionLOD = 0.0f;

        int DebugViz = 0;

        std::shared_ptr<Shader>       SkyboxShader;
        std::shared_ptr<VertexArray>  SkyboxVAO;
        std::shared_ptr<VertexBuffer> SkyboxVBO;

        std::shared_ptr<Shader>      TonemapShader;
        std::shared_ptr<VertexArray> TonemapVAO;

        std::vector<std::shared_ptr<Framebuffer>> BloomMips;
        uint32_t                BloomSceneWidth   = 0;
        uint32_t                BloomSceneHeight  = 0;
        std::shared_ptr<Shader> BloomDownsampleShader;
        std::shared_ptr<Shader> BloomUpsampleShader;
        std::shared_ptr<Shader> FXAAShader;
        bool                    FXAAEnabled       = true;
        uint32_t                BloomFinalTexture = 0;
        bool                    BloomEnabled      = true;
        float                   BloomThreshold    = 1.0f;
        float                   BloomIntensity    = 0.04f;

        // One depth-only framebuffer per cascade.
        std::shared_ptr<Framebuffer> ShadowFramebuffers[Renderer3D::kCascadeCount];
        glm::mat4                    LightVPs       [Renderer3D::kCascadeCount];
        float                        CascadeSplits  [Renderer3D::kCascadeCount] = { 0.0f, 0.0f, 0.0f, 0.0f };
        int                          ActiveCascade   = -1;
        bool                         ShadowsActive   = false; // set on any cascade; cleared at EndScene
        int                          PrevFBO         = 0;     // restored at last EndShadowPass
        int                          PrevViewport[4] = { 0, 0, 0, 0 };
    };

    static Renderer3DStorage sData;

    namespace {
        // Generates the Karis split-sum BRDF LUT (RG16F: axes = NdotV / roughness,
        // values = F = F0 * scale + bias). Runs once at Renderer3D::Init.
        GLuint GenerateBRDFLUT(uint32_t size) {
            const std::string  shader_path =
                Project::GetEngineAssetFileSystemPath("shaders/brdf_lut").generic_string();
            std::shared_ptr<Shader> shader = AssetManager::GetShader(shader_path);
            if (!shader) return 0;

            GLuint lut = 0;
            glCreateTextures(GL_TEXTURE_2D, 1, &lut);
            glTextureStorage2D(lut, 1, GL_RG16F, size, size);
            glTextureParameteri(lut, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
            glTextureParameteri(lut, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
            glTextureParameteri(lut, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(lut, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            // GL state leaking from this Init helper has caused regressions before
            // (forgetting to restore GL_BLEND made transparent grid lines opaque).
            // Snapshot everything we touch, restore on exit.
            GLint     prev_fbo = 0, prev_viewport[4] = { 0, 0, 0, 0 };
            GLint     prev_vao = 0;
            GLfloat   prev_clear_color[4] = { 0, 0, 0, 0 };
            GLboolean prev_color_mask[4]  = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
            GLboolean prev_depth, prev_cull, prev_blend;
            glGetIntegerv (GL_FRAMEBUFFER_BINDING,  &prev_fbo);
            glGetIntegerv (GL_VIEWPORT,             prev_viewport);
            glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &prev_vao);
            glGetFloatv   (GL_COLOR_CLEAR_VALUE,    prev_clear_color);
            glGetBooleanv (GL_COLOR_WRITEMASK,      prev_color_mask);
            prev_depth = glIsEnabled(GL_DEPTH_TEST);
            prev_cull  = glIsEnabled(GL_CULL_FACE);
            prev_blend = glIsEnabled(GL_BLEND);

            // Core profile requires a VAO bound even for gl_VertexID-only draws.
            GLuint fbo = 0, vao = 0;
            glCreateFramebuffers(1, &fbo);
            glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, lut, 0);
            // Explicit draw-buffer mapping: some drivers ship the FBO default as
            // GL_NONE despite the spec, which silently no-ops the draw.
            GLenum draw_bufs[] = { GL_COLOR_ATTACHMENT0 };
            glNamedFramebufferDrawBuffers(fbo, 1, draw_bufs);
            GLenum fb_status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER);
            if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
                LOOM_CORE_ERROR("IBL: BRDF LUT framebuffer incomplete (status 0x{:X})", fb_status);
                glDeleteFramebuffers(1, &fbo);
                glDeleteTextures(1, &lut);
                return 0;
            }
            glCreateVertexArrays(1, &vao);

            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glViewport(0, 0, size, size);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            shader->Bind();
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            // Sanity-check: zero center pixel means the draw silently no-op'd
            // (usually a shader linkage / draw-buffer issue) and specular IBL
            // will be black.
            float pixel[4] = { 0, 0, 0, 0 };
            glGetTextureSubImage(lut, 0, (GLint)(size / 2), (GLint)(size / 2), 0,
                                 1, 1, 1, GL_RGBA, GL_FLOAT, sizeof(pixel), pixel);
            if (pixel[0] == 0.0f && pixel[1] == 0.0f) {
                LOOM_CORE_ERROR("IBL: BRDF LUT pass wrote zero - split-sum specular will be black. "
                                "Check brdf_lut.{{vert,frag}} compile log + driver output.");
            } else {
                LOOM_CORE_TRACE("IBL: BRDF LUT center sample = ({}, {}) - generation OK.",
                                pixel[0], pixel[1]);
            }

            glDeleteFramebuffers(1, &fbo);
            glDeleteVertexArrays(1, &vao);

            if (prev_depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (prev_cull)  glEnable(GL_CULL_FACE);  else glDisable(GL_CULL_FACE);
            if (prev_blend) glEnable(GL_BLEND);      else glDisable(GL_BLEND);
            glColorMask(prev_color_mask[0], prev_color_mask[1], prev_color_mask[2], prev_color_mask[3]);
            glClearColor(prev_clear_color[0], prev_clear_color[1], prev_clear_color[2], prev_clear_color[3]);
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
            glBindVertexArray((GLuint)prev_vao);
            glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);

            LOOM_CORE_TRACE("IBL: BRDF LUT generated ({}x{} RG16F, split-sum)", size, size);
            return lut;
        }
    } // anonymous namespace

    void Renderer3D::Init() {
        std::string mesh_path   = Project::GetEngineAssetFileSystemPath("shaders/mesh").generic_string();
        sData.MeshShader        = AssetManager::GetShader(mesh_path);

        std::string skinned_path  = Project::GetEngineAssetFileSystemPath("shaders/mesh_skinned").generic_string();
        sData.MeshSkinnedShader   = AssetManager::GetShader(skinned_path);

        std::string shadow_path = Project::GetEngineAssetFileSystemPath("shaders/shadow_depth").generic_string();
        sData.ShadowShader      = AssetManager::GetShader(shadow_path);

        sData.WhiteTexture = Texture2D::Create(1, 1);
        uint32_t white     = 0xFFFFFFFF;
        sData.WhiteTexture->SetData(&white, sizeof(uint32_t));

        sData.FlatNormalTexture = Texture2D::Create(1, 1);
        // Little-endian: 0xFFFF8080 -> bytes [80,80,FF,FF] -> (128,128,255,255).
        uint32_t flat_normal    = 0xFFFF8080;
        sData.FlatNormalTexture->SetData(&flat_normal, sizeof(uint32_t));

        sData.CameraUniformBuffer = UniformBuffer::Create(sizeof(glm::mat4), 0);
        sData.BonesUniformBuffer  = UniformBuffer::Create(sizeof(glm::mat4) * Skeleton::kMaxJoints, 1);

        FramebufferSpecification shadow_spec;
        shadow_spec.Width       = kShadowMapSize;
        shadow_spec.Height      = kShadowMapSize;
        shadow_spec.Attachments = { FramebufferTextureFormat::DEPTH32F };
        for (int i = 0; i < kCascadeCount; ++i) {
            sData.ShadowFramebuffers[i] = Framebuffer::Create(shadow_spec);
        }

        // Both shaders share the same fragment + texture-unit layout.
        for (auto* shader : { &sData.MeshShader, &sData.MeshSkinnedShader }) {
            (*shader)->Bind();
            (*shader)->UploadUniformInt("uAlbedoTexture",            0);
            (*shader)->UploadUniformInt("uShadowMap0",               1);
            (*shader)->UploadUniformInt("uShadowMap1",               2);
            (*shader)->UploadUniformInt("uShadowMap2",               3);
            (*shader)->UploadUniformInt("uShadowMap3",               4);
            (*shader)->UploadUniformInt("uIrradianceMap",            5);
            (*shader)->UploadUniformInt("uPrefilterMap",             6);
            (*shader)->UploadUniformInt("uBRDFLUT",                  7);
            (*shader)->UploadUniformInt("uORMTexture",               8);
            (*shader)->UploadUniformInt("uEmissiveTexture",          9);
            (*shader)->UploadUniformInt("uNormalMapTexture",        10);
        }

        sData.BRDFLUT = GenerateBRDFLUT(512);

        // Skybox cube: 8 unique vertices, 36 indices.
        float skybox_vertices[] = {
            -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f
        };
        uint32_t skybox_indices[] = {
            1, 2, 6, 6, 5, 1, // Right
            0, 4, 7, 7, 3, 0, // Left
            3, 7, 6, 6, 2, 3, // Top
            0, 1, 5, 5, 4, 0, // Bottom
            5, 6, 7, 7, 4, 5, // Back
            1, 0, 3, 3, 2, 1  // Front
        };

        sData.SkyboxVAO = VertexArray::Create();
        sData.SkyboxVBO = VertexBuffer::Create(sizeof(skybox_vertices));
        sData.SkyboxVBO->SetData(skybox_vertices, sizeof(skybox_vertices));
        sData.SkyboxVBO->SetLayout({ { ShaderDataType::Float3, "aPosition" } });
        sData.SkyboxVAO->AddVertexBuffer(sData.SkyboxVBO);
        auto skybox_ibo = IndexBuffer::Create(skybox_indices, sizeof(skybox_indices) / sizeof(uint32_t));
        sData.SkyboxVAO->SetIndexBuffer(skybox_ibo);

        std::string skybox_shader_path = Project::GetEngineAssetFileSystemPath("shaders/skybox").generic_string();
        sData.SkyboxShader             = AssetManager::GetShader(skybox_shader_path);

        // Tonemap + bloom + FXAA use gl_VertexID-only fullscreen triangles;
        // core profile still requires a bound VAO for the draw to be valid.
        std::string tonemap_shader_path = Project::GetEngineAssetFileSystemPath("shaders/tonemap").generic_string();
        sData.TonemapShader = AssetManager::GetShader(tonemap_shader_path);
        sData.TonemapShader->Bind();
        sData.TonemapShader->UploadUniformInt("uHDRScene", 0);
        sData.TonemapShader->UploadUniformInt("uBloom",    1);
        sData.TonemapVAO = VertexArray::Create();

        std::string bloom_ds_path = Project::GetEngineAssetFileSystemPath("shaders/bloom_downsample").generic_string();
        sData.BloomDownsampleShader = AssetManager::GetShader(bloom_ds_path);
        sData.BloomDownsampleShader->Bind();
        sData.BloomDownsampleShader->UploadUniformInt("uSource", 0);

        std::string bloom_us_path = Project::GetEngineAssetFileSystemPath("shaders/bloom_upsample").generic_string();
        sData.BloomUpsampleShader = AssetManager::GetShader(bloom_us_path);
        sData.BloomUpsampleShader->Bind();
        sData.BloomUpsampleShader->UploadUniformInt("uSource", 0);

        std::string fxaa_path = Project::GetEngineAssetFileSystemPath("shaders/fxaa").generic_string();
        sData.FXAAShader = AssetManager::GetShader(fxaa_path);
        sData.FXAAShader->Bind();
        sData.FXAAShader->UploadUniformInt("uSource", 0);
    }

    void Renderer3D::Shutdown() {
        sData.MeshShader.reset();
        sData.MeshSkinnedShader.reset();
        sData.ShadowShader.reset();
        sData.WhiteTexture.reset();
        sData.FlatNormalTexture.reset();
        sData.CameraUniformBuffer.reset();
        sData.BonesUniformBuffer.reset();
        sData.SkyboxShader.reset();
        sData.SkyboxVAO.reset();
        sData.SkyboxVBO.reset();
        sData.TonemapShader.reset();
        sData.TonemapVAO.reset();
        sData.BloomDownsampleShader.reset();
        sData.BloomUpsampleShader.reset();
        sData.BloomMips.clear();
        sData.BloomFinalTexture = 0;
        sData.FXAAShader.reset();
        sData.IrradianceMap.reset();
        sData.PrefilterMap.reset();
        if (sData.BRDFLUT) {
            glDeleteTextures(1, &sData.BRDFLUT);
            sData.BRDFLUT = 0;
        }
        for (int i = 0; i < kCascadeCount; ++i) sData.ShadowFramebuffers[i].reset();
    }

    void Renderer3D::BeginScene(const EditorCamera& camera) {
        sData.CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
        sData.CameraUniformBuffer->SetData(&sData.CameraBuffer.ViewProjection, sizeof(glm::mat4));
        sData.ViewPosition = camera.GetPosition();

        sData.DirLightCount   = 0;
        sData.PointLightCount = 0;
    }

    void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform) {
        sData.CameraBuffer.ViewProjection = camera.GetProjectionMatrix() * glm::inverse(transform);
        sData.CameraUniformBuffer->SetData(&sData.CameraBuffer.ViewProjection, sizeof(glm::mat4));
        sData.ViewPosition = glm::vec3(transform[3]);

        sData.DirLightCount   = 0;
        sData.PointLightCount = 0;
    }

    void Renderer3D::EndScene() {
        sData.ShadowsActive = false;
    }

    void Renderer3D::SetCascadeSplits(const float splits[kCascadeCount]) {
        for (int i = 0; i < kCascadeCount; ++i) sData.CascadeSplits[i] = splits[i];
    }

    void Renderer3D::SetIrradianceMap(const std::shared_ptr<TextureCubemap>& irradiance) {
        sData.IrradianceMap = irradiance;
    }

    void Renderer3D::SetPrefilterMap(const std::shared_ptr<TextureCubemap>& prefilter) {
        sData.PrefilterMap = prefilter;
        // Shader samples textureLod(R, roughness * uMaxReflectionLOD), so this
        // must equal the cubemap's last mip index.
        if (prefilter && prefilter->GetMipLevels() > 0) {
            sData.MaxReflectionLOD = float(prefilter->GetMipLevels() - 1);
        } else {
            sData.MaxReflectionLOD = 0.0f;
        }
    }

    void Renderer3D::DrawSkybox(const glm::mat4& view, const glm::mat4& projection,
                                const std::shared_ptr<TextureCubemap>& cubemap) {
        if (!cubemap || !sData.SkyboxShader || !sData.SkyboxVAO) return;

        // Zero the translation so the cube stays centered on the camera.
        glm::mat4 view_no_trans = view;
        view_no_trans[3]        = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glm::mat4 vp            = projection * view_no_trans;

        sData.SkyboxShader->Bind();
        sData.SkyboxShader->UploadUniformMat4("uViewProjection", vp);
        sData.SkyboxShader->UploadUniformInt ("uSkybox",         0);
        cubemap->Bind(0);
        RenderCommand::DrawIndexed(sData.SkyboxVAO.get(), 36);
    }

    void Renderer3D::SetDebugViz(int mode) {
        sData.DebugViz = mode;
    }

    void Renderer3D::BeginShadowPass(int cascade_index, const glm::mat4& light_view_projection) {
        if (cascade_index < 0 || cascade_index >= kCascadeCount) return;
        if (!sData.ShadowFramebuffers[cascade_index] || !sData.ShadowShader) return;

        sData.LightVPs[cascade_index] = light_view_projection;
        sData.ActiveCascade           = cascade_index;
        sData.ShadowsActive           = true;

        // Save FBO+viewport on the first cascade only; cascades within a frame
        // share the same caller state, so restoring per-cascade would thrash.
        if (cascade_index == 0) {
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &sData.PrevFBO);
            glGetIntegerv(GL_VIEWPORT,            sData.PrevViewport);
        }

        sData.ShadowFramebuffers[cascade_index]->Bind();
        glClear(GL_DEPTH_BUFFER_BIT);

        sData.ShadowShader->Bind();
        sData.ShadowShader->UploadUniformMat4("uLightVP", light_view_projection);
    }

    void Renderer3D::SubmitShadow(const std::shared_ptr<MeshAsset>& mesh,
                                  const glm::mat4& transform) {
        if (!mesh || !mesh->GetVertexArray()) return;
        if (sData.ActiveCascade < 0)          return; // BeginShadowPass not called

        sData.ShadowShader->UploadUniformMat4("uModel", transform);

        const auto& vao = mesh->GetVertexArray();
        vao->Bind();
        RenderCommand::DrawIndexed(vao.get(), mesh->GetIndexCount());
    }

    void Renderer3D::EndShadowPass() {
        if (sData.ActiveCascade < 0) return;

        if (sData.ActiveCascade == kCascadeCount - 1) {
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)sData.PrevFBO);
            glViewport(sData.PrevViewport[0], sData.PrevViewport[1],
                       sData.PrevViewport[2], sData.PrevViewport[3]);
        }
        sData.ActiveCascade = -1;
    }

    void Renderer3D::SetLights(const DirectionalLight* dir_lights, int dir_count,
                               const PointLight*       point_lights, int point_count) {
        sData.DirLightCount = std::min(dir_count, kMaxDirectionalLights);
        for (int i = 0; i < sData.DirLightCount; ++i) {
            sData.DirLightDir[i]   = dir_lights[i].Direction;
            sData.DirLightColor[i] = dir_lights[i].Color;
        }

        sData.PointLightCount = std::min(point_count, kMaxPointLights);
        for (int i = 0; i < sData.PointLightCount; ++i) {
            sData.PointLightPos[i]   = point_lights[i].Position;
            sData.PointLightColor[i] = point_lights[i].Color;
            sData.PointLightRange[i] = point_lights[i].Range;
        }
    }

    void Renderer3D::Submit(const std::shared_ptr<MeshAsset>& mesh,
                            const glm::vec4& albedo_color,
                            const std::shared_ptr<Texture2D>& albedo_texture,
                            const std::shared_ptr<Texture2D>& orm_texture,
                            const std::shared_ptr<Texture2D>& emissive_texture,
                            const glm::vec3& emissive_factor,
                            const std::shared_ptr<Texture2D>& normal_texture,
                            const glm::mat4& transform,
                            float roughness,
                            float metallic,
                            int   entity_id,
                            const glm::mat4* sampled_local_transforms,
                            int   sampled_count) {
        if (!mesh || !mesh->GetVertexArray()) return;

        Shader* shader = mesh->IsSkinned() ? sData.MeshSkinnedShader.get()
                                           : sData.MeshShader.get();
        shader->Bind();

        // Upload skin matrices for skinned meshes. Bind-pose path (slice 1):
        // walk the skeleton top-down computing joint_world, then multiply by
        // each joint's inverseBind to get the skin matrix. At bind pose this
        // collapses to identity — but the math runs, so a Picasso-monster
        // means a real bug rather than just bad uniform plumbing.
        if (mesh->IsSkinned()) {
            const Skeleton& skel = mesh->GetSkeleton();
            const int n          = std::min(skel.JointCount(), Skeleton::kMaxJoints);

            // Pick local transform per joint: sampled value overrides bind
            // when the caller supplied one, otherwise fall back to LocalBind.
            auto local_for = [&](int i) -> const glm::mat4& {
                if (sampled_local_transforms && i < sampled_count)
                    return sampled_local_transforms[i];
                return skel.Joints[i].LocalBind;
            };

            glm::mat4 joint_world  [Skeleton::kMaxJoints];
            glm::mat4 skin_matrices[Skeleton::kMaxJoints];
            for (int i = 0; i < n; ++i) {
                const SkeletonJoint& j = skel.Joints[i];
                if (j.Parent < 0 || j.Parent >= i) {
                    // No parent OR parent appears later in the array (mis-ordered
                    // skin; glTF spec recommends but doesn't require parent-first).
                    // RootWorld folds in any non-joint ancestor transform above
                    // the root joint (e.g., Blender's Z-up to Y-up rotation).
                    joint_world[i] = skel.RootWorld * local_for(i);
                } else {
                    joint_world[i] = joint_world[j.Parent] * local_for(i);
                }
                skin_matrices[i] = joint_world[i] * j.InverseBind;
            }
            // Pad remaining slots — vertices with bogus joint indices outside [0,n)
            // were clamped at import time, so this is belt-and-suspenders.
            for (int i = n; i < Skeleton::kMaxJoints; ++i)
                skin_matrices[i] = glm::mat4(1.0f);

            sData.BonesUniformBuffer->SetData(skin_matrices, sizeof(skin_matrices));
        }

        shader->UploadUniformMat4  ("uModel",        transform);
        shader->UploadUniformFloat4("uAlbedoColor",  albedo_color);
        shader->UploadUniformFloat ("uRoughness",     roughness);
        shader->UploadUniformFloat ("uMetallic",      metallic);
        shader->UploadUniformFloat3("uEmissiveFactor", emissive_factor);
        shader->UploadUniformInt   ("uEntityID",      entity_id);

        shader->UploadUniformFloat3("uViewPos",      sData.ViewPosition);
        shader->UploadUniformInt   ("uDirLightCount",   sData.DirLightCount);
        shader->UploadUniformInt   ("uPointLightCount", sData.PointLightCount);
        if (sData.DirLightCount > 0) {
            shader->UploadUniformFloat3Array("uDirLightDir",   sData.DirLightDir,   sData.DirLightCount);
            shader->UploadUniformFloat3Array("uDirLightColor", sData.DirLightColor, sData.DirLightCount);
        }
        if (sData.PointLightCount > 0) {
            shader->UploadUniformFloat3Array("uPointLightPos",   sData.PointLightPos,   sData.PointLightCount);
            shader->UploadUniformFloat3Array("uPointLightColor", sData.PointLightColor, sData.PointLightCount);
            shader->UploadUniformFloatArray ("uPointLightRange", sData.PointLightRange, sData.PointLightCount);
        }

        bool has_ibl = sData.IrradianceMap || sData.PrefilterMap;
        shader->UploadUniformInt  ("uHasIBL",          has_ibl ? 1 : 0);
        shader->UploadUniformInt  ("uHasPrefilter",    sData.PrefilterMap ? 1 : 0);
        shader->UploadUniformFloat("uMaxReflectionLOD", sData.MaxReflectionLOD);
        if (sData.IrradianceMap) sData.IrradianceMap->Bind(5);
        if (sData.PrefilterMap)  sData.PrefilterMap->Bind(6);
        if (sData.BRDFLUT)       glBindTextureUnit(7, sData.BRDFLUT);

        shader->UploadUniformInt("uDebugViz", sData.DebugViz);

        shader->UploadUniformInt("uShadowsEnabled", sData.ShadowsActive ? 1 : 0);
        if (sData.ShadowsActive) {
            shader->UploadUniformMat4 ("uLightVP0",       sData.LightVPs[0]);
            shader->UploadUniformMat4 ("uLightVP1",       sData.LightVPs[1]);
            shader->UploadUniformMat4 ("uLightVP2",       sData.LightVPs[2]);
            shader->UploadUniformMat4 ("uLightVP3",       sData.LightVPs[3]);
            shader->UploadUniformFloatArray("uCascadeSplits", sData.CascadeSplits, kCascadeCount);

            for (int i = 0; i < kCascadeCount; ++i) {
                uint32_t tex = sData.ShadowFramebuffers[i]->GetDepthAttachmentRendererID();
                glActiveTexture(GL_TEXTURE1 + i);
                glBindTexture(GL_TEXTURE_2D, tex);
            }
            glActiveTexture(GL_TEXTURE0);
        }

        const auto& tex = albedo_texture ? albedo_texture : sData.WhiteTexture;
        tex->Bind(0);

        const auto& orm_tex = orm_texture ? orm_texture : sData.WhiteTexture;
        orm_tex->Bind(8);

        const auto& em_tex = emissive_texture ? emissive_texture : sData.WhiteTexture;
        em_tex->Bind(9);

        const auto& nrm_tex = normal_texture ? normal_texture : sData.FlatNormalTexture;
        nrm_tex->Bind(10);

        const auto& vao = mesh->GetVertexArray();
        vao->Bind();
        RenderCommand::DrawIndexed(vao.get(), mesh->GetIndexCount());
    }

    void Renderer3D::Tonemap(uint32_t hdr_color_texture_id) {
        sData.TonemapShader->Bind();
        glBindTextureUnit(0, hdr_color_texture_id);

        const int has_bloom = (sData.BloomEnabled && sData.BloomFinalTexture != 0) ? 1 : 0;
        if (has_bloom) glBindTextureUnit(1, sData.BloomFinalTexture);
        sData.TonemapShader->UploadUniformInt  ("uHasBloom",       has_bloom);
        sData.TonemapShader->UploadUniformFloat("uBloomIntensity", sData.BloomIntensity);

        sData.TonemapVAO->Bind();
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    void Renderer3D::SetBloomEnabled(bool enabled)     { sData.BloomEnabled   = enabled; }
    void Renderer3D::SetBloomThreshold(float v)        { sData.BloomThreshold = v; }
    void Renderer3D::SetBloomIntensity(float v)        { sData.BloomIntensity = v; }

    void Renderer3D::FXAAPass(uint32_t source_color_texture, uint32_t width, uint32_t height) {
        sData.FXAAShader->Bind();
        glBindTextureUnit(0, source_color_texture);
        sData.FXAAShader->UploadUniformFloat2("uTexelSize",
            glm::vec2(1.0f / float(width), 1.0f / float(height)));
        sData.TonemapVAO->Bind();
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    void Renderer3D::SetFXAAEnabled(bool enabled) { sData.FXAAEnabled = enabled; }
    bool Renderer3D::IsFXAAEnabled()              { return sData.FXAAEnabled; }

    void Renderer3D::BloomPass(uint32_t hdr_color_texture_id,
                               uint32_t scene_width, uint32_t scene_height) {
        if (!sData.BloomEnabled || scene_width < 4 || scene_height < 4) {
            sData.BloomFinalTexture = 0;
            return;
        }

        if (scene_width != sData.BloomSceneWidth || scene_height != sData.BloomSceneHeight) {
            sData.BloomMips.clear();
            constexpr int kMaxMips = 6;
            uint32_t w = scene_width  / 2;
            uint32_t h = scene_height / 2;
            for (int i = 0; i < kMaxMips; ++i) {
                if (w < 4 || h < 4) break;
                FramebufferSpecification spec;
                spec.Attachments = { FramebufferTextureFormat::RGBA16F };
                spec.Width  = w;
                spec.Height = h;
                sData.BloomMips.push_back(Framebuffer::Create(spec));
                w /= 2; h /= 2;
            }
            sData.BloomSceneWidth  = scene_width;
            sData.BloomSceneHeight = scene_height;
        }

        if (sData.BloomMips.empty()) {
            sData.BloomFinalTexture = 0;
            return;
        }

        GLboolean prev_blend     = glIsEnabled(GL_BLEND);
        GLboolean prev_depth     = glIsEnabled(GL_DEPTH_TEST);
        GLint     prev_blend_src = 0, prev_blend_dst = 0;
        glGetIntegerv(GL_BLEND_SRC, &prev_blend_src);
        glGetIntegerv(GL_BLEND_DST, &prev_blend_dst);
        GLint prev_fbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);
        GLint prev_viewport[4];
        glGetIntegerv(GL_VIEWPORT, prev_viewport);

        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);

        // ── Downsample chain ──
        sData.BloomDownsampleShader->Bind();
        sData.BloomDownsampleShader->UploadUniformFloat("uThreshold", sData.BloomThreshold);
        sData.TonemapVAO->Bind();

        uint32_t src_tex    = hdr_color_texture_id;
        uint32_t src_width  = scene_width;
        uint32_t src_height = scene_height;
        for (size_t i = 0; i < sData.BloomMips.size(); ++i) {
            sData.BloomMips[i]->Bind();
            sData.BloomDownsampleShader->UploadUniformInt   ("uPrefilter",
                                                             (i == 0) ? 1 : 0);
            sData.BloomDownsampleShader->UploadUniformFloat2("uSrcTexelSize",
                glm::vec2(1.0f / float(src_width), 1.0f / float(src_height)));
            glBindTextureUnit(0, src_tex);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            const auto& spec = sData.BloomMips[i]->GetSpecification();
            src_tex    = sData.BloomMips[i]->GetColorAttachmentRendererID(0);
            src_width  = spec.Width;
            src_height = spec.Height;
        }

        // ── Upsample chain (3x3 tent, additive over downsample content) ──
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        sData.BloomUpsampleShader->Bind();
        sData.TonemapVAO->Bind();
        for (int i = (int)sData.BloomMips.size() - 2; i >= 0; --i) {
            auto& current_mip = sData.BloomMips[i];
            auto& smaller_mip = sData.BloomMips[i + 1];

            current_mip->Bind();
            const auto& smaller_spec = smaller_mip->GetSpecification();
            sData.BloomUpsampleShader->UploadUniformFloat2("uSrcTexelSize",
                glm::vec2(1.0f / float(smaller_spec.Width),
                          1.0f / float(smaller_spec.Height)));
            glBindTextureUnit(0, smaller_mip->GetColorAttachmentRendererID(0));
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        sData.BloomFinalTexture = sData.BloomMips[0]->GetColorAttachmentRendererID(0);

        if (prev_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (prev_depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glBlendFunc((GLenum)prev_blend_src, (GLenum)prev_blend_dst);
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
        glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
    }

} // namespace Loom
