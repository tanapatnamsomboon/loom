#include "loom/renderer/renderer_3d.h"
#include "loom/asset/asset_manager.h"
#include "loom/project/project.h"
#include "loom/renderer/render_command.h"
#include "loom/renderer/shader.h"
#include <algorithm>

namespace Loom {

    struct Renderer3DStorage {
        std::shared_ptr<Shader>    MeshShader;
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
    };

    static Renderer3DStorage sData;

    void Renderer3D::Init() {
        std::string shader_path = Project::GetEngineAssetFileSystemPath("shaders/mesh").generic_string();
        sData.MeshShader        = AssetManager::GetShader(shader_path);

        sData.WhiteTexture = Texture2D::Create(1, 1);
        uint32_t white     = 0xFFFFFFFF;
        sData.WhiteTexture->SetData(&white, sizeof(uint32_t));

        sData.CameraUniformBuffer = UniformBuffer::Create(sizeof(glm::mat4), 0);

        // The albedo sampler binds to texture unit 0 unconditionally.
        sData.MeshShader->Bind();
        sData.MeshShader->UploadUniformInt("uAlbedoTexture", 0);
    }

    void Renderer3D::Shutdown() {
        sData.MeshShader.reset();
        sData.WhiteTexture.reset();
        sData.CameraUniformBuffer.reset();
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
        // Nothing to flush — submissions draw immediately.
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

        const auto& tex = albedo_texture ? albedo_texture : sData.WhiteTexture;
        tex->Bind(0);

        const auto& vao = mesh->GetVertexArray();
        vao->Bind();
        RenderCommand::DrawIndexed(vao.get(), mesh->GetIndexCount());
    }

} // namespace Loom
