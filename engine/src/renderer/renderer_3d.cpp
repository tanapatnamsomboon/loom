#include "loom/renderer/renderer_3d.h"
#include "loom/asset/asset_manager.h"
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
        // Bound at texture unit 5 in Submit (units 0=albedo, 1-4=shadow cascades).
        std::shared_ptr<TextureCubemap> IrradianceMap;

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
        // IBL irradiance on 5 (prefilter/BRDF LUT slots reserved at 6/7 for later).
        sData.MeshShader->Bind();
        sData.MeshShader->UploadUniformInt("uAlbedoTexture",  0);
        sData.MeshShader->UploadUniformInt("uShadowMap0",     1);
        sData.MeshShader->UploadUniformInt("uShadowMap1",     2);
        sData.MeshShader->UploadUniformInt("uShadowMap2",     3);
        sData.MeshShader->UploadUniformInt("uShadowMap3",     4);
        sData.MeshShader->UploadUniformInt("uIrradianceMap",  5);

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
                            const glm::mat4& transform,
                            float roughness,
                            float metallic,
                            int   entity_id) {
        if (!mesh || !mesh->GetVertexArray()) return;

        sData.MeshShader->Bind();

        // Per-draw uniforms
        sData.MeshShader->UploadUniformMat4  ("uModel",        transform);
        sData.MeshShader->UploadUniformFloat4("uAlbedoColor",  albedo_color);
        sData.MeshShader->UploadUniformFloat ("uRoughness",    roughness);
        sData.MeshShader->UploadUniformFloat ("uMetallic",     metallic);
        sData.MeshShader->UploadUniformInt   ("uEntityID",     entity_id);

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

        // IBL — bind irradiance cubemap at unit 5 + flag the shader.
        sData.MeshShader->UploadUniformInt("uHasIBL", sData.IrradianceMap ? 1 : 0);
        if (sData.IrradianceMap) {
            sData.IrradianceMap->Bind(5);
        }

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

        const auto& vao = mesh->GetVertexArray();
        vao->Bind();
        RenderCommand::DrawIndexed(vao.get(), mesh->GetIndexCount());
    }

} // namespace Loom
