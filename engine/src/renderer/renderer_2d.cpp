#include "loom/renderer/renderer_2d.h"
#include "loom/renderer/vertex_array.h"
#include "loom/renderer/shader.h"
#include "loom/renderer/render_command.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Loom {

    struct Renderer2DStorage {
        std::shared_ptr<VertexArray> QuadVertexArray;
        std::shared_ptr<VertexBuffer> QuadVBO;
        std::shared_ptr<IndexBuffer> QuadIBO;
        std::shared_ptr<Shader> TextureShader;
        std::shared_ptr<Texture2D> WhiteTexture;
    };

    static Renderer2DStorage* sData;

    void Renderer2D::Init() {
        sData = new Renderer2DStorage();

        sData->QuadVertexArray.reset(VertexArray::Create());

        float square_vertices[5 * 4] = {
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
             0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
             0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
            -0.5f,  0.5f, 0.0f, 0.0f, 1.0f
        };

        sData->QuadVBO = std::shared_ptr<VertexBuffer>(VertexBuffer::Create(square_vertices, sizeof(square_vertices)));
        sData->QuadVBO->SetLayout({
            { ShaderDataType::Float3, "aPosition" },
            { ShaderDataType::Float2, "aTexCoord" }
        });
        sData->QuadVertexArray->AddVertexBuffer(sData->QuadVBO.get());

        uint32_t square_indices[6] = { 0, 1, 2, 2, 3, 0 };
        sData->QuadIBO = std::shared_ptr<IndexBuffer>(IndexBuffer::Create(square_indices, sizeof(square_indices) / sizeof(uint32_t)));
        sData->QuadVertexArray->SetIndexBuffer(sData->QuadIBO.get());

        sData->WhiteTexture.reset(Texture2D::Create(1, 1));
        uint32_t white_texture_data = 0xFFFFFFFF;
        sData->WhiteTexture->SetData(&white_texture_data, sizeof(uint32_t));

        sData->TextureShader.reset(Shader::Create("assets/shaders/texture"));
        sData->TextureShader->Bind();
        sData->TextureShader->UploadUniformInt("uTexture", 0);

    }

    void Renderer2D::Shutdown() {
        delete sData;
    }

    void Renderer2D::BeginScene(const OrthographicCamera& camera) {
        sData->TextureShader->Bind();
        sData->TextureShader->UploadUniformMat4("uViewProjection", camera.GetViewProjectionMatrix());
    }

    void Renderer2D::EndScene() {
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) {
        DrawQuad({ position.x, position.y, 0.0f }, size, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color) {
        sData->TextureShader->Bind();
        sData->TextureShader->UploadUniformFloat4("uColor", color);
        sData->WhiteTexture->Bind();

        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
        sData->TextureShader->UploadUniformMat4("uTransform", transform);

        sData->QuadVertexArray->Bind();
        RenderCommand::DrawIndexed(sData->QuadVertexArray.get());
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const std::shared_ptr<Texture2D>& texture) {
        DrawQuad({ position.x, position.y, 0.0f }, size, texture);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const std::shared_ptr<Texture2D>& texture) {
        sData->TextureShader->Bind();
        sData->TextureShader->UploadUniformFloat4("uColor", glm::vec4(1.0f));
        texture->Bind();

        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
        sData->TextureShader->UploadUniformMat4("uTransform", transform);

        sData->QuadVertexArray->Bind();
        RenderCommand::DrawIndexed(sData->QuadVertexArray.get());
    }

} // namespace Loom