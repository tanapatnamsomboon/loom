#include "loom/renderer/renderer_3d.h"
#include "loom/asset/asset_manager.h"
#include "loom/project/project.h"
#include "loom/renderer/render_command.h"
#include "loom/renderer/shader.h"

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
    }

    void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform) {
        sData.CameraBuffer.ViewProjection = camera.GetProjectionMatrix() * glm::inverse(transform);
        sData.CameraUniformBuffer->SetData(&sData.CameraBuffer.ViewProjection, sizeof(glm::mat4));
    }

    void Renderer3D::EndScene() {
        // Nothing to flush — submissions draw immediately.
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
        sData.MeshShader->UploadUniformMat4("uModel",        transform);
        sData.MeshShader->UploadUniformFloat4("uAlbedoColor", albedo_color);
        sData.MeshShader->UploadUniformFloat("uRoughness",    roughness);
        sData.MeshShader->UploadUniformFloat("uMetallic",     metallic);
        sData.MeshShader->UploadUniformInt("uEntityID",       entity_id);

        const auto& tex = albedo_texture ? albedo_texture : sData.WhiteTexture;
        tex->Bind(0);

        const auto& vao = mesh->GetVertexArray();
        vao->Bind();
        RenderCommand::DrawIndexed(vao.get(), mesh->GetIndexCount());
    }

} // namespace Loom
