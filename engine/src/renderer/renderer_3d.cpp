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
        std::shared_ptr<Shader>    ShadowShader;
        std::shared_ptr<Texture2D> WhiteTexture;

        // View-projection bound at UBO slot 0 (shared with Renderer2D — both write
        // it at BeginScene; last-write-wins is fine since they are called in
        // strict sequence per frame).
        struct CameraData {
            glm::mat4 ViewProjection;
        };
        CameraData                     CameraBuffer;
        std::shared_ptr<UniformBuffer> CameraUniformBuffer;

        glm::vec3 ViewPosition = glm::vec3(0.0f);

        // Scratch storage for SoA upload to the shader.
        glm::vec3 DirLightDir[Renderer3D::kMaxDirectionalLights];
        glm::vec3 DirLightColor[Renderer3D::kMaxDirectionalLights];
        int       DirLightCount = 0;

        glm::vec3 PointLightPos[Renderer3D::kMaxPointLights];
        glm::vec3 PointLightColor[Renderer3D::kMaxPointLights];
        float     PointLightRange[Renderer3D::kMaxPointLights];
        int       PointLightCount = 0;

        // ── IBL state ──
        // Texture unit layout in Submit:
        //   0     = albedo
        //   1-4   = shadow cascades
        //   5     = irradiance cubemap (diffuse ambient)
        //   6     = prefiltered env cubemap (specular IBL, mips = roughness)
        //   7     = BRDF LUT (split-sum scale+bias, generated once at Init)
        std::shared_ptr<TextureCubemap> IrradianceMap;
        std::shared_ptr<TextureCubemap> PrefilterMap;
        // BRDF LUT is an RG16F 2D texture owned as raw GL state — Texture2D's
        // abstraction is RGBA8-only and adding a format-enum just for this
        // single-instance global texture isn't worth it. Generated at Init,
        // freed at Shutdown.
        GLuint                          BRDFLUT      = 0;
        // log2(prefilter face size) — uploaded to the shader so it knows the
        // max LOD to clamp roughness against. 0 when no prefilter is bound.
        float                           MaxReflectionLOD = 0.0f;

        // Debug visualization mode (see mesh.frag uDebugViz). 0 = normal PBR.
        int DebugViz = 0;

        // ── Skybox state ──
        // Unit cube + skybox shader owned here so both the editor and scene
        // play-mode paths can call DrawSkybox without duplicating setup.
        std::shared_ptr<Shader>       SkyboxShader;
        std::shared_ptr<VertexArray>  SkyboxVAO;
        std::shared_ptr<VertexBuffer> SkyboxVBO;

        // ── Shadow state (cascaded) ──
        // One framebuffer per cascade. Sized to a square depth texture each;
        // mesh.frag has kCascadeCount sampler2D uniforms bound at units 1..N.
        std::shared_ptr<Framebuffer> ShadowFramebuffers[Renderer3D::kCascadeCount];
        glm::mat4                    LightVPs       [Renderer3D::kCascadeCount];
        float                        CascadeSplits  [Renderer3D::kCascadeCount] = { 0.0f, 0.0f, 0.0f, 0.0f };
        // Index of cascade currently being rendered (between Begin/EndShadowPass).
        int                          ActiveCascade   = -1;
        bool                         ShadowsActive   = false; // any cascade rendered this frame; consumed by Submit; cleared at EndScene
        // Saved framebuffer + viewport restored at EndShadowPass.
        int                          PrevFBO         = 0;
        int                          PrevViewport[4] = { 0, 0, 0, 0 };
    };

    static Renderer3DStorage sData;

    namespace {
        // Generates the BRDF LUT for the Karis split-sum IBL approximation —
        // 512×512 RG16F, axes = (NdotV, roughness), values = (scale, bias) for
        // F = F0 * scale + bias. Runs once at Renderer3D::Init.
        GLuint GenerateBRDFLUT(uint32_t size) {
            constexpr GLuint   kBindTextureUnit = 7; // matches the mesh-shader uniform
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

            // Snapshot caller state so this Init helper restores cleanly.
            // GL state leaked from this pass into long-lived render state has
            // bitten us once already: forgetting to restore GL_BLEND turned
            // transparent grid / icon edges opaque for the rest of the
            // session. Save everything we touch, restore on the way out.
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

            // Scratch FBO + VAO (core profile requires a VAO bound even for
            // gl_VertexID-only draws; the brdf_lut.vert reads no attributes).
            GLuint fbo = 0, vao = 0;
            glCreateFramebuffers(1, &fbo);
            glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, lut, 0);
            // Explicit draw-buffer mapping. New FBOs technically default to
            // GL_COLOR_ATTACHMENT0, but a handful of drivers (and some debug
            // captures) have shipped with that initial state set to GL_NONE.
            // Setting it explicitly is free and rules the class of bug out.
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

            // Guard against color-write masks leaking in from earlier draws.
            // (None should at engine-init time, but Renderer3D::Init runs
            // after Application has touched GL once, so we're paranoid.)
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

            // Verify a non-zero pixel landed. If the texture is still all
            // zero after the draw, something silently no-op'd (most often
            // a shader linkage / draw-buffer issue) and the LUT-sampled
            // specular IBL will be invisible — surface the error early.
            float pixel[4] = { 0, 0, 0, 0 };
            glGetTextureSubImage(lut, 0, (GLint)(size / 2), (GLint)(size / 2), 0,
                                 1, 1, 1, GL_RGBA, GL_FLOAT, sizeof(pixel), pixel);
            if (pixel[0] == 0.0f && pixel[1] == 0.0f) {
                LOOM_CORE_ERROR("IBL: BRDF LUT pass wrote zero — split-sum specular will be black. "
                                "Check brdf_lut.{{vert,frag}} compile log + driver output.");
            } else {
                LOOM_CORE_TRACE("IBL: BRDF LUT center sample = ({}, {}) — generation OK.",
                                pixel[0], pixel[1]);
            }

            glDeleteFramebuffers(1, &fbo);
            glDeleteVertexArrays(1, &vao);

            // Restore caller state.
            if (prev_depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (prev_cull)  glEnable(GL_CULL_FACE);  else glDisable(GL_CULL_FACE);
            if (prev_blend) glEnable(GL_BLEND);      else glDisable(GL_BLEND);
            glColorMask(prev_color_mask[0], prev_color_mask[1], prev_color_mask[2], prev_color_mask[3]);
            glClearColor(prev_clear_color[0], prev_clear_color[1], prev_clear_color[2], prev_clear_color[3]);
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
            glBindVertexArray((GLuint)prev_vao);
            glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);

            (void)kBindTextureUnit;
            LOOM_CORE_TRACE("IBL: BRDF LUT generated ({}x{} RG16F, split-sum)", size, size);
            return lut;
        }
    } // anonymous namespace

    void Renderer3D::Init() {
        std::string mesh_path   = Project::GetEngineAssetFileSystemPath("shaders/mesh").generic_string();
        sData.MeshShader        = AssetManager::GetShader(mesh_path);

        std::string shadow_path = Project::GetEngineAssetFileSystemPath("shaders/shadow_depth").generic_string();
        sData.ShadowShader      = AssetManager::GetShader(shadow_path);

        sData.WhiteTexture = Texture2D::Create(1, 1);
        uint32_t white     = 0xFFFFFFFF;
        sData.WhiteTexture->SetData(&white, sizeof(uint32_t));

        sData.CameraUniformBuffer = UniformBuffer::Create(sizeof(glm::mat4), 0);

        // One depth-only shadow framebuffer per cascade. DEPTH32F, no color attachments.
        FramebufferSpecification shadow_spec;
        shadow_spec.Width       = kShadowMapSize;
        shadow_spec.Height      = kShadowMapSize;
        shadow_spec.Attachments = { FramebufferTextureFormat::DEPTH32F };
        for (int i = 0; i < kCascadeCount; ++i) {
            sData.ShadowFramebuffers[i] = Framebuffer::Create(shadow_spec);
        }

        // Bind sampler units once: albedo on 0, cascade shadows on 1..kCascadeCount,
        // IBL irradiance on 5, prefilter cubemap on 6, BRDF LUT on 7,
        // ORM map on 8 (R=AO, G=rough, B=metal), emissive on 9.
        sData.MeshShader->Bind();
        sData.MeshShader->UploadUniformInt("uAlbedoTexture",            0);
        sData.MeshShader->UploadUniformInt("uShadowMap0",               1);
        sData.MeshShader->UploadUniformInt("uShadowMap1",               2);
        sData.MeshShader->UploadUniformInt("uShadowMap2",               3);
        sData.MeshShader->UploadUniformInt("uShadowMap3",               4);
        sData.MeshShader->UploadUniformInt("uIrradianceMap",            5);
        sData.MeshShader->UploadUniformInt("uPrefilterMap",             6);
        sData.MeshShader->UploadUniformInt("uBRDFLUT",                  7);
        sData.MeshShader->UploadUniformInt("uORMTexture",               8);
        sData.MeshShader->UploadUniformInt("uEmissiveTexture",          9);

        // BRDF LUT — environment-independent, generated once at engine init.
        sData.BRDFLUT = GenerateBRDFLUT(512);

        // Skybox cube: 8 unique vertices, 36 indices via IBO. Same layout the
        // editor used to keep inline — moved here so the play-mode path can
        // also draw a skybox without duplicating geometry/shader setup.
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
    }

    void Renderer3D::Shutdown() {
        sData.MeshShader.reset();
        sData.ShadowShader.reset();
        sData.WhiteTexture.reset();
        sData.CameraUniformBuffer.reset();
        sData.SkyboxShader.reset();
        sData.SkyboxVAO.reset();
        sData.SkyboxVBO.reset();
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

        // Reset light state; SetLights is called per-frame to refill.
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
        // Shadow state only applies to the 3D pass we just ran. Clear it so
        // any subsequent BeginScene without a matching shadow pass is shadow-free.
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
        // The shader samples `textureLod(uPrefilterMap, R, roughness * uMaxReflectionLOD)`,
        // so MaxLOD must match the cubemap's last mip index. For a face_size
        // of N, mip count is floor(log2(N)) + 1 — the last mip's LOD index
        // is mip_count - 1.
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

        // Save current FBO + viewport on the FIRST cascade only — restoring on
        // every EndShadowPass would thrash, and the saved values are identical
        // for back-to-back cascades within one frame.
        if (cascade_index == 0) {
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &sData.PrevFBO);
            glGetIntegerv(GL_VIEWPORT,            sData.PrevViewport);
        }

        sData.ShadowFramebuffers[cascade_index]->Bind(); // also sets viewport to shadow map size
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

        // Restore the caller's framebuffer + viewport on the LAST cascade only.
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
                            const glm::mat4& transform,
                            float roughness,
                            float metallic,
                            int   entity_id) {
        if (!mesh || !mesh->GetVertexArray()) return;

        sData.MeshShader->Bind();

        // Per-draw uniforms
        sData.MeshShader->UploadUniformMat4  ("uModel",        transform);
        sData.MeshShader->UploadUniformFloat4("uAlbedoColor",  albedo_color);
        sData.MeshShader->UploadUniformFloat ("uRoughness",     roughness);
        sData.MeshShader->UploadUniformFloat ("uMetallic",      metallic);
        sData.MeshShader->UploadUniformFloat3("uEmissiveFactor", emissive_factor);
        sData.MeshShader->UploadUniformInt   ("uEntityID",      entity_id);

        // Per-frame uniforms (cheap to re-upload; keeps Submit self-sufficient
        // even if SetLights / camera state changes mid-frame).
        sData.MeshShader->UploadUniformFloat3("uViewPos",      sData.ViewPosition);
        sData.MeshShader->UploadUniformInt   ("uDirLightCount",   sData.DirLightCount);
        sData.MeshShader->UploadUniformInt   ("uPointLightCount", sData.PointLightCount);
        if (sData.DirLightCount > 0) {
            sData.MeshShader->UploadUniformFloat3Array("uDirLightDir",   sData.DirLightDir,   sData.DirLightCount);
            sData.MeshShader->UploadUniformFloat3Array("uDirLightColor", sData.DirLightColor, sData.DirLightCount);
        }
        if (sData.PointLightCount > 0) {
            sData.MeshShader->UploadUniformFloat3Array("uPointLightPos",   sData.PointLightPos,   sData.PointLightCount);
            sData.MeshShader->UploadUniformFloat3Array("uPointLightColor", sData.PointLightColor, sData.PointLightCount);
            sData.MeshShader->UploadUniformFloatArray ("uPointLightRange", sData.PointLightRange, sData.PointLightCount);
        }

        // IBL — bind irradiance (5), prefilter (6), BRDF LUT (7). The `uHasIBL`
        // gate flips on when *any* of the three is present; the shader handles
        // a missing prefilter/LUT by skipping just the specular IBL term. In
        // practice the scene either has the full triple bound or nothing.
        bool has_ibl = sData.IrradianceMap || sData.PrefilterMap;
        sData.MeshShader->UploadUniformInt  ("uHasIBL",          has_ibl ? 1 : 0);
        sData.MeshShader->UploadUniformInt  ("uHasPrefilter",    sData.PrefilterMap ? 1 : 0);
        sData.MeshShader->UploadUniformFloat("uMaxReflectionLOD", sData.MaxReflectionLOD);
        if (sData.IrradianceMap) sData.IrradianceMap->Bind(5);
        if (sData.PrefilterMap)  sData.PrefilterMap->Bind(6);
        if (sData.BRDFLUT)       glBindTextureUnit(7, sData.BRDFLUT);

        // Debug visualization mode (0 = normal PBR path).
        sData.MeshShader->UploadUniformInt("uDebugViz", sData.DebugViz);

        // Shadow uniforms — only meaningful when at least one cascade ran this frame.
        sData.MeshShader->UploadUniformInt("uShadowsEnabled", sData.ShadowsActive ? 1 : 0);
        if (sData.ShadowsActive) {
            // Per-cascade VP matrices + far-plane splits (used by the frag shader
            // to pick which cascade to sample based on view-space depth).
            sData.MeshShader->UploadUniformMat4 ("uLightVP0",       sData.LightVPs[0]);
            sData.MeshShader->UploadUniformMat4 ("uLightVP1",       sData.LightVPs[1]);
            sData.MeshShader->UploadUniformMat4 ("uLightVP2",       sData.LightVPs[2]);
            sData.MeshShader->UploadUniformMat4 ("uLightVP3",       sData.LightVPs[3]);
            sData.MeshShader->UploadUniformFloatArray("uCascadeSplits", sData.CascadeSplits, kCascadeCount);

            // Bind all cascade depth textures to units 1..N.
            for (int i = 0; i < kCascadeCount; ++i) {
                uint32_t tex = sData.ShadowFramebuffers[i]->GetDepthAttachmentRendererID();
                glActiveTexture(GL_TEXTURE1 + i);
                glBindTexture(GL_TEXTURE_2D, tex);
            }
            glActiveTexture(GL_TEXTURE0); // restore the conventional active unit
        }

        const auto& tex = albedo_texture ? albedo_texture : sData.WhiteTexture;
        tex->Bind(0);

        // ORM map on unit 8 (R=AO, G=roughness, B=metallic). White fallback
        // => no occlusion + factors pass through unchanged (G/B == 1.0). The
        // shader always samples it, no per-draw "has map" branch.
        const auto& orm_tex = orm_texture ? orm_texture : sData.WhiteTexture;
        orm_tex->Bind(8);

        // Emissive on unit 9. White fallback × zero EmissiveFactor still mutes
        // the term, so the common no-emissive case has no extra cost.
        const auto& em_tex = emissive_texture ? emissive_texture : sData.WhiteTexture;
        em_tex->Bind(9);

        const auto& vao = mesh->GetVertexArray();
        vao->Bind();
        RenderCommand::DrawIndexed(vao.get(), mesh->GetIndexCount());
    }

} // namespace Loom
