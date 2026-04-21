#include "loom/renderer/renderer_2d.h"
#include "loom/renderer/vertex_array.h"
#include "loom/renderer/shader.h"
#include "loom/renderer/render_command.h"
#include <glm/gtc/matrix_transform.hpp>
#include <array>

namespace Loom {

    struct QuadVertex {
        glm::vec3 Position;
        glm::vec4 Color;
        glm::vec2 TexCoord;
        float     TexIndex;
        float     TilingFactor;
    };

    struct Renderer2DStorage {
        static constexpr uint32_t MaxQuads        = 10'000;
        static constexpr uint32_t MaxVertices     = MaxQuads * 4;
        static constexpr uint32_t MaxIndices      = MaxQuads * 6;
        static constexpr uint32_t MaxTextureSlots = 32;

        std::shared_ptr<VertexArray>    QuadVertexArray;
        std::shared_ptr<VertexBuffer>   QuadVertexBuffer;
        std::shared_ptr<Shader>         TextureShader;
        std::shared_ptr<Texture2D>      WhiteTexture;

        uint32_t    QuadIndexCount          = 0;
        QuadVertex* QuadVertexBufferBase    = nullptr;
        QuadVertex* QuadVertexBufferPtr     = nullptr;

        std::array<std::shared_ptr<Texture2D>, MaxTextureSlots> TextureSlots;
        uint32_t TextureSlotIndex = 1;

        glm::vec4 QuadVertexPositions[4];
    };

    static Renderer2DStorage sData;

    void Renderer2D::Init() {
        sData.QuadVertexArray = VertexArray::Create();

        sData.QuadVertexBuffer = VertexBuffer::Create(sData.MaxVertices * sizeof(QuadVertex));
        sData.QuadVertexBuffer->SetLayout({
            { ShaderDataType::Float3, "aPosition"    },
            { ShaderDataType::Float4, "aColor"       },
            { ShaderDataType::Float2, "aTexCoord"    },
            { ShaderDataType::Float,  "aTexIndex"    },
            { ShaderDataType::Float,  "aTilingFactor"}
        });
        sData.QuadVertexArray->AddVertexBuffer(sData.QuadVertexBuffer);

        sData.QuadVertexBufferBase = new QuadVertex[sData.MaxVertices];

        uint32_t* quad_indices = new uint32_t[sData.MaxIndices];
        uint32_t offset = 0;
        for (uint32_t i = 0; i < sData.MaxIndices; i += 6) {
            quad_indices[i + 0] = offset + 0;
            quad_indices[i + 1] = offset + 1;
            quad_indices[i + 2] = offset + 2;
            quad_indices[i + 3] = offset + 2;
            quad_indices[i + 4] = offset + 3;
            quad_indices[i + 5] = offset + 0;
            offset += 4;
        }

        auto ibo = IndexBuffer::Create(quad_indices, sData.MaxIndices);
        sData.QuadVertexArray->SetIndexBuffer(ibo);
        delete[] quad_indices;

        sData.TextureSlots[0] = Texture2D::Create(1, 1);
        uint32_t white_texture_data = 0xffffffff;
        sData.TextureSlots[0]->SetData(&white_texture_data, sizeof(uint32_t));

        sData.TextureShader = Shader::Create("assets/shaders/texture");

        int32_t samplers[sData.MaxTextureSlots];
        for (uint32_t i = 0; i < sData.MaxTextureSlots; i++)
            samplers[i] = i;

        sData.TextureShader->Bind();
        sData.TextureShader->UploadUniformIntArray("uTextures", samplers, sData.MaxTextureSlots);

        sData.QuadVertexPositions[0] = { -0.5f, -0.5f,  0.0f,  1.0f };
        sData.QuadVertexPositions[1] = {  0.5f, -0.5f,  0.0f,  1.0f };
        sData.QuadVertexPositions[2] = {  0.5f,  0.5f,  0.0f,  1.0f };
        sData.QuadVertexPositions[3] = { -0.5f,  0.5f,  0.0f,  1.0f };
    }

    void Renderer2D::Shutdown() {
    }

    void Renderer2D::BeginScene(const OrthographicCamera& camera) {
        sData.TextureShader->Bind();
        sData.TextureShader->UploadUniformMat4("uViewProjection", camera.GetViewProjectionMatrix());

        sData.QuadIndexCount = 0;
        sData.QuadVertexBufferPtr = sData.QuadVertexBufferBase;
        sData.TextureSlotIndex = 1;
    }

    void Renderer2D::BeginScene(const EditorCamera& camera) {
        sData.TextureShader->Bind();
        sData.TextureShader->UploadUniformMat4("uViewProjection", camera.GetViewProjectionMatrix());

        sData.QuadIndexCount = 0;
        sData.QuadVertexBufferPtr = sData.QuadVertexBufferBase;
        sData.TextureSlotIndex = 1;
    }

    void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform) {
        glm::mat4 view_proj = camera.GetProjection() * glm::inverse(transform);

        sData.TextureShader->Bind();
        sData.TextureShader->UploadUniformMat4("uViewProjection", view_proj);

        sData.QuadIndexCount = 0;
        sData.QuadVertexBufferPtr = sData.QuadVertexBufferBase;
        sData.TextureSlotIndex = 1;
    }

    void Renderer2D::EndScene() {
        Flush();
    }

    void Renderer2D::Flush() {
        if (sData.QuadIndexCount) {
            for (uint32_t i = 0; i < sData.TextureSlotIndex; i++) {
                sData.TextureSlots[i]->Bind(i);
            }

            uint32_t data_size = (uint8_t*)sData.QuadVertexBufferPtr - (uint8_t*)sData.QuadVertexBufferBase;
            sData.QuadVertexBuffer->SetData(sData.QuadVertexBufferBase, data_size);
            RenderCommand::DrawIndexed(sData.QuadVertexArray.get(), sData.QuadIndexCount);
        }
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) {
        DrawQuad({ position.x, position.y, 0.0f }, size, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color) {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
                            * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

        constexpr size_t quad_vertex_count = 4;
        constexpr float texture_index = 0.0f;
        constexpr glm::vec2 texture_coords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        constexpr float tiling_factor = 1.0f;

        for (size_t i = 0; i < quad_vertex_count; i++) {
            sData.QuadVertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            sData.QuadVertexBufferPtr->Color        = color;
            sData.QuadVertexBufferPtr->TexCoord     = texture_coords[i];
            sData.QuadVertexBufferPtr->TexIndex     = texture_index;
            sData.QuadVertexBufferPtr->TilingFactor = tiling_factor;
            sData.QuadVertexBufferPtr++;
        }

        sData.QuadIndexCount += 6;
    }

    void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color) {
        constexpr size_t quad_vertex_count = 4;
        constexpr float texture_index = 0.0f;
        constexpr glm::vec2 texture_coords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        constexpr float tiling_factor = 1.0f;

        for (size_t i = 0; i < quad_vertex_count; i++) {
            sData.QuadVertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            sData.QuadVertexBufferPtr->Color        = color;
            sData.QuadVertexBufferPtr->TexCoord     = texture_coords[i];
            sData.QuadVertexBufferPtr->TexIndex     = texture_index;
            sData.QuadVertexBufferPtr->TilingFactor = tiling_factor;
            sData.QuadVertexBufferPtr++;
        }

        sData.QuadIndexCount += 6;
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const std::shared_ptr<Texture2D>& texture, const glm::vec4& tint_color) {
        DrawQuad({ position.x, position.y, 0.0f }, size, texture, tint_color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const std::shared_ptr<Texture2D>& texture, const glm::vec4& tint_color) {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
                            * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

        float texture_index = 0.0f;
        for (uint32_t i = 1; i < sData.TextureSlotIndex; i++) {
            if (sData.TextureSlots[i].get() == texture.get()) {
                texture_index = (float)i;
                break;
            }
        }

        if (texture_index == 0.0f) {
            texture_index = (float)sData.TextureSlotIndex;
            sData.TextureSlots[sData.TextureSlotIndex] = texture;
            sData.TextureSlotIndex++;
        }

        constexpr size_t quad_vertex_count = 4;
        constexpr glm::vec2 texture_coords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        constexpr float tiling_factor = 1.0f;

        for (size_t i = 0; i < quad_vertex_count; i++) {
            sData.QuadVertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            sData.QuadVertexBufferPtr->Color        = tint_color;
            sData.QuadVertexBufferPtr->TexCoord     = texture_coords[i];
            sData.QuadVertexBufferPtr->TexIndex     = texture_index;
            sData.QuadVertexBufferPtr->TilingFactor = tiling_factor;
            sData.QuadVertexBufferPtr++;
        }

        sData.QuadIndexCount += 6;
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const std::shared_ptr<SubTexture2D>& sub_texture, const glm::vec4& tint_color) {
        DrawQuad({ position.x, position.y, 0.0f }, size, sub_texture, tint_color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const std::shared_ptr<SubTexture2D>& sub_texture, const glm::vec4& tint_color) {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
                            * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

        const std::shared_ptr<Texture2D> texture = sub_texture->GetTexture();
        const glm::vec2* texture_coords = sub_texture->GetTexCoords();
        constexpr size_t quad_vertex_count = 4;

        float texture_index = 0.0f;
        for (uint32_t i = 1; i < sData.TextureSlotIndex; i++) {
            if (sData.TextureSlots[i].get() == texture.get()) {
                texture_index = (float)i;
                break;
            }
        }

        if (texture_index == 0.0f) {
            texture_index = (float)sData.TextureSlotIndex;
            sData.TextureSlots[sData.TextureSlotIndex] = texture;
            sData.TextureSlotIndex++;
        }

        for (int i = 0; i < quad_vertex_count; i++) {
            sData.QuadVertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            sData.QuadVertexBufferPtr->Color        = tint_color;
            sData.QuadVertexBufferPtr->TexCoord     = texture_coords[i];
            sData.QuadVertexBufferPtr->TexIndex     = texture_index;
            sData.QuadVertexBufferPtr->TilingFactor = 1.0f;
            sData.QuadVertexBufferPtr++;
        }

        sData.QuadIndexCount += 6;
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color) {
        DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color) {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
                            * glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
                            * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

        constexpr size_t quad_vertex_count = 4;
        constexpr float texture_index = 0.0f;
        constexpr glm::vec2 texture_coords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        constexpr float tiling_factor = 1.0f;

        for (size_t i = 0; i < quad_vertex_count; i++) {
            sData.QuadVertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            sData.QuadVertexBufferPtr->Color        = color;
            sData.QuadVertexBufferPtr->TexCoord     = texture_coords[i];
            sData.QuadVertexBufferPtr->TexIndex     = texture_index;
            sData.QuadVertexBufferPtr->TilingFactor = tiling_factor;
            sData.QuadVertexBufferPtr++;
        }

        sData.QuadIndexCount += 6;
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const std::shared_ptr<Texture2D>& texture, const glm::vec4& tint_color) {
        DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, texture, tint_color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const std::shared_ptr<Texture2D>& texture, const glm::vec4& tint_color) {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
                            * glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
                            * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

        float texture_index = 0.0f;
        for (uint32_t i = 1; i < sData.TextureSlotIndex; i++) {
            if (sData.TextureSlots[i].get() == texture.get()) {
                texture_index = (float)i;
                break;
            }
        }

        if (texture_index == 0.0f) {
            texture_index = (float)sData.TextureSlotIndex;
            sData.TextureSlots[sData.TextureSlotIndex] = texture;
            sData.TextureSlotIndex++;
        }

        constexpr size_t quad_vertex_count = 4;
        constexpr glm::vec2 texture_coords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        constexpr float tiling_factor = 1.0f;

        for (size_t i = 0; i < quad_vertex_count; i++) {
            sData.QuadVertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            sData.QuadVertexBufferPtr->Color        = tint_color;
            sData.QuadVertexBufferPtr->TexCoord     = texture_coords[i];
            sData.QuadVertexBufferPtr->TexIndex     = texture_index;
            sData.QuadVertexBufferPtr->TilingFactor = tiling_factor;
            sData.QuadVertexBufferPtr++;
        }

        sData.QuadIndexCount += 6;
    }

} // namespace Loom