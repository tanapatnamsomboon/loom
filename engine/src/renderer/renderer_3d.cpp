#include "loom/renderer/renderer_3d.h"
#include "loom/asset/asset_manager.h"
#include "loom/project/project.h"
#include "loom/renderer/framebuffer.h"
#include "loom/renderer/render_command.h"
#include "loom/renderer/shader.h"
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

        // ── Shadow state ──
        std::shared_ptr<Framebuffer> ShadowFramebuffer;
        glm::mat4                    LightVP       = glm::mat4(1.0f);
        bool                         ShadowsActive = false; // true between Begin/EndShadowPass and consumed by Submit; cleared at EndScene
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

        // Depth-only shadow framebuffer (DEPTH32F, no color attachments).
        FramebufferSpecification shadow_spec;
        shadow_spec.Width       = kShadowMapSize;
        shadow_spec.Height      = kShadowMapSize;
        shadow_spec.Attachments = { FramebufferTextureFormat::DEPTH32F };
        sData.ShadowFramebuffer = Framebuffer::Create(shadow_spec);

        // Bind sampler units once: albedo on 0, shadow on 1.
        sData.MeshShader->Bind();
        sData.MeshShader->UploadUniformInt("uAlbedoTexture", 0);
        sData.MeshShader->UploadUniformInt("uShadowMap",     1);
    }

    void Renderer3D::Shutdown() {
        sData.MeshShader.reset();
        sData.ShadowShader.reset();
        sData.WhiteTexture.reset();
        sData.CameraUniformBuffer.reset();
        sData.ShadowFramebuffer.reset();
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

    void Renderer3D::BeginShadowPass(const glm::mat4& light_view_projection) {
        if (!sData.ShadowFramebuffer || !sData.ShadowShader) return;

        sData.LightVP       = light_view_projection;
        sData.ShadowsActive = true;

        // Save current FBO + viewport so EndShadowPass can restore them after
        // the depth pass mutates GL state.
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &sData.PrevFBO);
        glGetIntegerv(GL_VIEWPORT,            sData.PrevViewport);

        // No culling tweak: front-face culling makes the shadow map record the
        // caster's back face (its underside), which sits flush with the ground
        // at the contact point and produces an unfixable "shadow gap" without a
        // negative bias. Rendering all faces (default) records the front face,
        // so the contact point shadows cleanly; slope-scale bias in mesh.frag
        // handles the residual acne.

        sData.ShadowFramebuffer->Bind(); // also sets viewport to shadow map size
        glClear(GL_DEPTH_BUFFER_BIT);

        sData.ShadowShader->Bind();
        sData.ShadowShader->UploadUniformMat4("uLightVP", sData.LightVP);
    }

    void Renderer3D::SubmitShadow(const std::shared_ptr<MeshAsset>& mesh,
                                  const glm::mat4& transform) {
        if (!mesh || !mesh->GetVertexArray()) return;
        if (!sData.ShadowsActive)             return; // BeginShadowPass not called

        sData.ShadowShader->UploadUniformMat4("uModel", transform);

        const auto& vao = mesh->GetVertexArray();
        vao->Bind();
        RenderCommand::DrawIndexed(vao.get(), mesh->GetIndexCount());
    }

    void Renderer3D::EndShadowPass() {
        if (!sData.ShadowsActive) return;

        // Restore the caller's framebuffer + viewport. ShadowsActive stays true
        // so the upcoming Submit() calls bind the shadow map + uLightVP.
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)sData.PrevFBO);
        glViewport(sData.PrevViewport[0], sData.PrevViewport[1],
                   sData.PrevViewport[2], sData.PrevViewport[3]);
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

        // Shadow uniforms — only meaningful when BeginShadowPass ran this frame.
        sData.MeshShader->UploadUniformInt("uShadowsEnabled", sData.ShadowsActive ? 1 : 0);
        if (sData.ShadowsActive) {
            sData.MeshShader->UploadUniformMat4("uLightVP", sData.LightVP);
            uint32_t shadow_tex = sData.ShadowFramebuffer->GetDepthAttachmentRendererID();
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, shadow_tex);
            glActiveTexture(GL_TEXTURE0); // restore the conventional active unit
        }

        const auto& tex = albedo_texture ? albedo_texture : sData.WhiteTexture;
        tex->Bind(0);

        const auto& vao = mesh->GetVertexArray();
        vao->Bind();
        RenderCommand::DrawIndexed(vao.get(), mesh->GetIndexCount());
    }

} // namespace Loom
