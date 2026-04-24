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
        int       EntityID;
    };

    struct CircleVertex {
        glm::vec3 Position;
        glm::vec3 LocalPosition;
        glm::vec4 Color;
        float     Thickness;
        float     Fade;
        int       EntityID;
    };

    struct LineVertex {
        glm::vec3 Position;
        glm::vec4 Color;
        int       EntityID;
    };

    struct Renderer2DStorage {
        static constexpr uint32_t MaxQuads        = 10'000;
        static constexpr uint32_t MaxVertices     = MaxQuads * 4;
        static constexpr uint32_t MaxIndices      = MaxQuads * 6;
        static constexpr uint32_t MaxTextureSlots = 32;

        // Quad Data
        std::shared_ptr<VertexArray>    QuadVertexArray;
        std::shared_ptr<VertexBuffer>   QuadVertexBuffer;
        std::shared_ptr<Shader>         QuadShader;

        uint32_t    QuadIndexCount          = 0;
        QuadVertex* QuadVertexBufferBase    = nullptr;
        QuadVertex* QuadVertexBufferPtr     = nullptr;

        glm::vec4 QuadVertexPositions[4];

        std::shared_ptr<Texture2D> WhiteTexture;
        std::array<std::shared_ptr<Texture2D>, MaxTextureSlots> TextureSlots;
        uint32_t TextureSlotIndex = 1;

        // Circle Data
        std::shared_ptr<VertexArray>    CircleVertexArray;
        std::shared_ptr<VertexBuffer>   CircleVertexBuffer;
        std::shared_ptr<Shader>         CircleShader;

        uint32_t      CircleIndexCount          = 0;
        CircleVertex* CircleVertexBufferBase    = nullptr;
        CircleVertex* CircleVertexBufferPtr     = nullptr;

        // Line Data
        std::shared_ptr<VertexArray>    LineVertexArray;
        std::shared_ptr<VertexBuffer>   LineVertexBuffer;
        std::shared_ptr<Shader>         LineShader;

        uint32_t    LineVertexCount          = 0;
        LineVertex* LineVertexBufferBase    = nullptr;
        LineVertex* LineVertexBufferPtr     = nullptr;

        // Other
        struct CameraData {
            glm::mat4 ViewProjection;
        };
        CameraData CameraBuffer;
        std::shared_ptr<UniformBuffer> CameraUniformBuffer;
    };

    static Renderer2DStorage sData;

    void Renderer2D::Init() {
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

        {
            sData.QuadVertexArray = VertexArray::Create();

            sData.QuadVertexBuffer = VertexBuffer::Create(sData.MaxVertices * sizeof(QuadVertex));
            sData.QuadVertexBuffer->SetLayout({
                { ShaderDataType::Float3, "aPosition"    },
                { ShaderDataType::Float4, "aColor"       },
                { ShaderDataType::Float2, "aTexCoord"    },
                { ShaderDataType::Float,  "aTexIndex"    },
                { ShaderDataType::Float,  "aTilingFactor"},
                { ShaderDataType::Int,    "aEntityID"    }
            });
            sData.QuadVertexArray->AddVertexBuffer(sData.QuadVertexBuffer);

            sData.QuadVertexBufferBase = new QuadVertex[sData.MaxVertices];

            sData.QuadVertexArray->SetIndexBuffer(ibo);

            sData.QuadVertexPositions[0] = { -0.5f, -0.5f,  0.0f,  1.0f };
            sData.QuadVertexPositions[1] = {  0.5f, -0.5f,  0.0f,  1.0f };
            sData.QuadVertexPositions[2] = {  0.5f,  0.5f,  0.0f,  1.0f };
            sData.QuadVertexPositions[3] = { -0.5f,  0.5f,  0.0f,  1.0f };

            sData.QuadShader = Shader::Create("assets/shaders/quad");

            int32_t samplers[sData.MaxTextureSlots];
            for (uint32_t i = 0; i < sData.MaxTextureSlots; i++)
                samplers[i] = i;

            sData.QuadShader->Bind();
            sData.QuadShader->UploadUniformIntArray("uTextures", samplers, sData.MaxTextureSlots);

            sData.TextureSlots[0] = Texture2D::Create(1, 1);
            uint32_t white_texture_data = 0xffffffff;
            sData.TextureSlots[0]->SetData(&white_texture_data, sizeof(uint32_t));
        }

        {
            sData.CircleVertexArray = VertexArray::Create();

            sData.CircleVertexBuffer = VertexBuffer::Create(sData.MaxVertices * sizeof(CircleVertex));
            sData.CircleVertexBuffer->SetLayout({
                { ShaderDataType::Float3, "aPosition"     },
                { ShaderDataType::Float3, "aLocalPosition"},
                { ShaderDataType::Float4, "aColor"        },
                { ShaderDataType::Float,  "aThickness"    },
                { ShaderDataType::Float,  "aFade"         },
                { ShaderDataType::Int,    "aEntityID"     }
            });
            sData.CircleVertexArray->AddVertexBuffer(sData.CircleVertexBuffer);

            sData.CircleVertexArray->SetIndexBuffer(ibo);
            sData.CircleVertexBufferBase = new CircleVertex[sData.MaxVertices];
            sData.CircleShader = Shader::Create("assets/shaders/circle");
        }

        {
            sData.LineVertexArray = VertexArray::Create();

            sData.LineVertexBuffer = VertexBuffer::Create(sData.MaxVertices * sizeof(LineVertex));
            sData.LineVertexBuffer->SetLayout({
                { ShaderDataType::Float3, "aPosition" },
                { ShaderDataType::Float4, "aColor"    },
                { ShaderDataType::Int,    "aEntityID" }
            });
            sData.LineVertexArray->AddVertexBuffer(sData.LineVertexBuffer);

            sData.LineVertexBufferBase = new LineVertex[sData.MaxVertices];

            sData.LineShader = Shader::Create("assets/shaders/line");
        }

        sData.CameraUniformBuffer = UniformBuffer::Create(sizeof(glm::mat4), 0);

        delete[] quad_indices;
    }

    void Renderer2D::Shutdown() {
    }

    void Renderer2D::BeginScene(const OrthographicCamera& camera) {
        sData.CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
        sData.CameraUniformBuffer->SetData(&sData.CameraBuffer.ViewProjection, sizeof(glm::mat4));

        sData.QuadShader->Bind();
        sData.QuadIndexCount = 0;
        sData.QuadVertexBufferPtr = sData.QuadVertexBufferBase;
        sData.TextureSlotIndex = 1;

        sData.CircleIndexCount = 0;
        sData.CircleVertexBufferPtr = sData.CircleVertexBufferBase;

        sData.LineVertexCount = 0;
        sData.LineVertexBufferPtr = sData.LineVertexBufferBase;
    }

    void Renderer2D::BeginScene(const EditorCamera& camera) {
        sData.CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
        sData.CameraUniformBuffer->SetData(&sData.CameraBuffer.ViewProjection, sizeof(glm::mat4));

        sData.QuadShader->Bind();
        sData.QuadIndexCount = 0;
        sData.QuadVertexBufferPtr = sData.QuadVertexBufferBase;
        sData.TextureSlotIndex = 1;

        sData.CircleIndexCount = 0;
        sData.CircleVertexBufferPtr = sData.CircleVertexBufferBase;

        sData.LineVertexCount = 0;
        sData.LineVertexBufferPtr = sData.LineVertexBufferBase;
    }

    void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform) {
        sData.CameraBuffer.ViewProjection = camera.GetProjectionMatrix() * glm::inverse(transform);
        sData.CameraUniformBuffer->SetData(&sData.CameraBuffer.ViewProjection, sizeof(glm::mat4));

        sData.QuadShader->Bind();
        sData.QuadIndexCount = 0;
        sData.QuadVertexBufferPtr = sData.QuadVertexBufferBase;
        sData.TextureSlotIndex = 1;

        sData.CircleIndexCount = 0;
        sData.CircleVertexBufferPtr = sData.CircleVertexBufferBase;

        sData.LineVertexCount = 0;
        sData.LineVertexBufferPtr = sData.LineVertexBufferBase;
    }

    void Renderer2D::EndScene() {
        Flush();
    }

    void Renderer2D::Flush() {
        if (sData.QuadIndexCount) {
            uint32_t data_size = (uint8_t*)sData.QuadVertexBufferPtr - (uint8_t*)sData.QuadVertexBufferBase;
            sData.QuadVertexBuffer->SetData(sData.QuadVertexBufferBase, data_size);

            for (uint32_t i = 0; i < sData.TextureSlotIndex; i++) {
                sData.TextureSlots[i]->Bind(i);
            }

            sData.QuadShader->Bind();
            RenderCommand::DrawIndexed(sData.QuadVertexArray.get(), sData.QuadIndexCount);
        }

        if (sData.CircleIndexCount) {
            uint32_t data_size = (uint8_t*)sData.CircleVertexBufferPtr - (uint8_t*)sData.CircleVertexBufferBase;
            sData.CircleVertexBuffer->SetData(sData.CircleVertexBufferBase, data_size);

            sData.CircleShader->Bind();
            RenderCommand::DrawIndexed(sData.CircleVertexArray.get(), sData.CircleIndexCount);
        }

        if (sData.LineVertexCount) {
            uint32_t data_size = (uint8_t*)sData.LineVertexBufferPtr - (uint8_t*)sData.LineVertexBufferBase;
            sData.LineVertexBuffer->SetData(sData.LineVertexBufferBase, data_size);

            sData.LineShader->Bind();
            RenderCommand::DrawLines(sData.LineVertexArray.get(), sData.LineVertexCount);
        }
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) {
        DrawQuad({ position.x, position.y, 0.0f }, size, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color) {
        if (sData.QuadIndexCount >= Renderer2DStorage::MaxIndices) {
            NextBatch();
        }

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
            sData.QuadVertexBufferPtr->EntityID     = -1;
            sData.QuadVertexBufferPtr++;
        }

        sData.QuadIndexCount += 6;
    }

    void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entity_id) {
        if (sData.QuadIndexCount >= Renderer2DStorage::MaxIndices) {
            NextBatch();
        }

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
            sData.QuadVertexBufferPtr->EntityID     = entity_id;
            sData.QuadVertexBufferPtr++;
        }

        sData.QuadIndexCount += 6;
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const std::shared_ptr<Texture2D>& texture, const glm::vec4& tint_color) {
        DrawQuad({ position.x, position.y, 0.0f }, size, texture, tint_color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const std::shared_ptr<Texture2D>& texture, const glm::vec4& tint_color) {
        if (sData.QuadIndexCount >= Renderer2DStorage::MaxIndices) {
            NextBatch();
        }

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
            sData.QuadVertexBufferPtr->EntityID     = -1;
            sData.QuadVertexBufferPtr++;
        }

        sData.QuadIndexCount += 6;
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const std::shared_ptr<SubTexture2D>& sub_texture, const glm::vec4& tint_color) {
        DrawQuad({ position.x, position.y, 0.0f }, size, sub_texture, tint_color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const std::shared_ptr<SubTexture2D>& sub_texture, const glm::vec4& tint_color) {
        if (sData.QuadIndexCount >= Renderer2DStorage::MaxIndices) {
            NextBatch();
        }

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
            sData.QuadVertexBufferPtr->EntityID     = -1;
            sData.QuadVertexBufferPtr++;
        }

        sData.QuadIndexCount += 6;
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color) {
        DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color) {
        if (sData.QuadIndexCount >= Renderer2DStorage::MaxIndices) {
            NextBatch();
        }

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
            sData.QuadVertexBufferPtr->EntityID     = -1;
            sData.QuadVertexBufferPtr++;
        }

        sData.QuadIndexCount += 6;
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const std::shared_ptr<Texture2D>& texture, const glm::vec4& tint_color) {
        DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, texture, tint_color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const std::shared_ptr<Texture2D>& texture, const glm::vec4& tint_color) {
        if (sData.QuadIndexCount >= Renderer2DStorage::MaxIndices) {
            NextBatch();
        }

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
            sData.QuadVertexBufferPtr->EntityID     = -1;
            sData.QuadVertexBufferPtr++;
        }

        sData.QuadIndexCount += 6;
    }

    void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int entity_id) {
        if (sData.CircleIndexCount >= Renderer2DStorage::MaxIndices) {
            NextBatch();
        }

        constexpr size_t circle_vertex_count = 4;

        for (size_t i = 0; i < circle_vertex_count; i++) {
            sData.CircleVertexBufferPtr->Position      = transform * sData.QuadVertexPositions[i];
            sData.CircleVertexBufferPtr->LocalPosition = sData.QuadVertexPositions[i] * 2.0f; // Scale to -1.0 -> 1.0
            sData.CircleVertexBufferPtr->Color         = color;
            sData.CircleVertexBufferPtr->Thickness     = thickness;
            sData.CircleVertexBufferPtr->Fade          = fade;
            sData.CircleVertexBufferPtr->EntityID      = entity_id;
            sData.CircleVertexBufferPtr++;
        }

        sData.CircleIndexCount += 6;
    }

    void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entity_id) {
        if (sData.LineVertexCount >= Renderer2DStorage::MaxVertices) {
            NextBatch();
        }

        sData.LineVertexBufferPtr->Position = p0;
        sData.LineVertexBufferPtr->Color    = color;
        sData.LineVertexBufferPtr->EntityID = entity_id;
        sData.LineVertexBufferPtr++;

        sData.LineVertexBufferPtr->Position = p1;
        sData.LineVertexBufferPtr->Color    = color;
        sData.LineVertexBufferPtr->EntityID = entity_id;
        sData.LineVertexBufferPtr++;

        sData.LineVertexCount += 2;
    }

    void Renderer2D::NextBatch() {
        Flush();

        sData.QuadIndexCount = 0;
        sData.QuadVertexBufferPtr = sData.QuadVertexBufferBase;
        sData.TextureSlotIndex = 1;

        sData.CircleIndexCount = 0;
        sData.CircleVertexBufferPtr = sData.CircleVertexBufferBase;

        sData.LineVertexCount = 0;
        sData.LineVertexBufferPtr = sData.LineVertexBufferBase;
    }

} // namespace Loom