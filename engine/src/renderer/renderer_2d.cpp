#include "loom/renderer/renderer_2d.h"
#include "loom/renderer/render_command.h"
#include "loom/renderer/shader.h"
#include "loom/renderer/vertex_array.h"
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <functional>

namespace Loom {

    template<typename VertexType>
    struct Batcher {
        uint32_t MaxVertices;

        std::shared_ptr<VertexArray>  VertexArray;
        std::shared_ptr<VertexBuffer> VertexBuffer;
        std::shared_ptr<Shader>       Shader;

        uint32_t IndexCount  = 0;
        uint32_t VertexCount = 0;

        VertexType* VertexBufferBase = nullptr;
        VertexType* VertexBufferPtr  = nullptr;

        std::function<void()> FlushCallback;

        Batcher(uint32_t max_quads, bool is_indexed = true)
            : MaxVertices(max_quads * (is_indexed ? 4 : 2)) {}

        void SetFlushCallback(std::function<void()> callback) {
            FlushCallback = std::move(callback);
        }

        bool IsFull(uint32_t vertices_to_add = 4) const {
            return (VertexBufferPtr + vertices_to_add) > (VertexBufferBase + MaxVertices);
        }

        void FlushIfFull(uint32_t vertices_to_add = 4) {
            if (IsFull(vertices_to_add)) {
                if (FlushCallback) {
                    FlushCallback();
                }
            }
        }

        void Reset() {
            IndexCount      = 0;
            VertexCount     = 0;
            VertexBufferPtr = VertexBufferBase;
        }
    };

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

        Batcher<QuadVertex>   Quads;
        Batcher<CircleVertex> Circles;
        Batcher<LineVertex>   Lines;

        glm::vec4 QuadVertexPositions[4];

        std::shared_ptr<Texture2D>                              WhiteTexture;
        std::array<std::shared_ptr<Texture2D>, MaxTextureSlots> TextureSlots;
        uint32_t                                                TextureSlotIndex = 1;

        struct CameraData {
            glm::mat4 ViewProjection;
        };
        CameraData                     CameraBuffer;
        std::shared_ptr<UniformBuffer> CameraUniformBuffer;

        Renderer2DStorage()
            : Quads(MaxQuads, true)
            , Circles(MaxQuads, true)
            , Lines(MaxQuads * 2, false) {}
    };

    static Renderer2DStorage sData;

    void Renderer2D::Init() {
        uint32_t* quad_indices = new uint32_t[sData.MaxIndices];
        uint32_t  offset       = 0;
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
        delete[] quad_indices;

        sData.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
        sData.QuadVertexPositions[1] = { 0.5f, -0.5f, 0.0f, 1.0f };
        sData.QuadVertexPositions[2] = { 0.5f, 0.5f, 0.0f, 1.0f };
        sData.QuadVertexPositions[3] = { -0.5f, 0.5f, 0.0f, 1.0f };

        //////////////////////////
        //      Quad Setup      //
        //////////////////////////
        auto& q        = sData.Quads;
        q.VertexArray  = VertexArray::Create();
        q.VertexBuffer = VertexBuffer::Create(sData.MaxVertices * sizeof(QuadVertex));
        q.VertexBuffer->SetLayout({ { ShaderDataType::Float3, "aPosition" },
                                    { ShaderDataType::Float4, "aColor" },
                                    { ShaderDataType::Float2, "aTexCoord" },
                                    { ShaderDataType::Float, "aTexIndex" },
                                    { ShaderDataType::Float, "aTilingFactor" },
                                    { ShaderDataType::Int, "aEntityID" } });
        q.VertexArray->AddVertexBuffer(q.VertexBuffer);
        q.VertexArray->SetIndexBuffer(ibo);
        q.VertexBufferBase = new QuadVertex[sData.MaxVertices];
        q.Shader           = Shader::Create("assets/shaders/quad");

        int32_t samplers[sData.MaxTextureSlots];
        for (uint32_t i = 0; i < sData.MaxTextureSlots; i++)
            samplers[i] = i;
        q.Shader->Bind();
        q.Shader->UploadUniformIntArray("uTextures", samplers, sData.MaxTextureSlots);

        /////////////////////////////
        //      White texture      //
        /////////////////////////////
        sData.WhiteTexture = Texture2D::Create(1, 1);
        uint32_t white     = 0xFFFFFFFF;
        sData.WhiteTexture->SetData(&white, sizeof(uint32_t));
        sData.TextureSlots[0] = sData.WhiteTexture;

        ////////////////////////////
        //      Circle Setup      //
        ////////////////////////////
        auto& c        = sData.Circles;
        c.VertexArray  = VertexArray::Create();
        c.VertexBuffer = VertexBuffer::Create(sData.MaxVertices * sizeof(CircleVertex));
        c.VertexBuffer->SetLayout({ { ShaderDataType::Float3, "aPosition" },
                                    { ShaderDataType::Float3, "aLocalPosition" },
                                    { ShaderDataType::Float4, "aColor" },
                                    { ShaderDataType::Float, "aThickness" },
                                    { ShaderDataType::Float, "aFade" },
                                    { ShaderDataType::Int, "aEntityID" } });
        c.VertexArray->AddVertexBuffer(c.VertexBuffer);
        c.VertexArray->SetIndexBuffer(ibo);
        c.VertexBufferBase = new CircleVertex[sData.MaxVertices];
        c.Shader           = Shader::Create("assets/shaders/circle");

        //////////////////////////
        //      Line Setup      //
        //////////////////////////
        auto& l        = sData.Lines;
        l.VertexArray  = VertexArray::Create();
        l.VertexBuffer = VertexBuffer::Create(sData.MaxVertices * sizeof(LineVertex));
        l.VertexBuffer->SetLayout({ { ShaderDataType::Float3, "aPosition" },
                                    { ShaderDataType::Float4, "aColor" },
                                    { ShaderDataType::Int, "aEntityID" } });
        l.VertexArray->AddVertexBuffer(l.VertexBuffer);
        l.VertexBufferBase = new LineVertex[sData.MaxVertices];
        l.Shader           = Shader::Create("assets/shaders/line");

        sData.Quads.SetFlushCallback([]() { Renderer2D::NextBatch(); });
        sData.Circles.SetFlushCallback([]() { Renderer2D::NextBatch(); });
        sData.Lines.SetFlushCallback([]() { Renderer2D::NextBatch(); });

        sData.CameraUniformBuffer = UniformBuffer::Create(sizeof(glm::mat4), 0);
    }

    void Renderer2D::Shutdown() {
    }

    void Renderer2D::BeginScene(const OrthographicCamera& camera) {
        sData.CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
        sData.CameraUniformBuffer->SetData(&sData.CameraBuffer.ViewProjection, sizeof(glm::mat4));

        ResetBatches();
    }

    void Renderer2D::BeginScene(const EditorCamera& camera) {
        sData.CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
        sData.CameraUniformBuffer->SetData(&sData.CameraBuffer.ViewProjection, sizeof(glm::mat4));

        ResetBatches();
    }

    void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform) {
        sData.CameraBuffer.ViewProjection = camera.GetProjectionMatrix() * glm::inverse(transform);
        sData.CameraUniformBuffer->SetData(&sData.CameraBuffer.ViewProjection, sizeof(glm::mat4));

        ResetBatches();
    }

    void Renderer2D::EndScene() {
        Flush();
    }

    void Renderer2D::Flush() {
        /////////////////////
        //      Quads      //
        /////////////////////
        if (sData.Quads.IndexCount) {
            uint32_t size = (uint8_t*)sData.Quads.VertexBufferPtr - (uint8_t*)sData.Quads.VertexBufferBase;
            sData.Quads.VertexBuffer->SetData(sData.Quads.VertexBufferBase, size);

            for (uint32_t i = 0; i < sData.TextureSlotIndex; ++i)
                sData.TextureSlots[i]->Bind(i);

            sData.Quads.Shader->Bind();
            RenderCommand::DrawIndexed(sData.Quads.VertexArray.get(), sData.Quads.IndexCount);
        }

        //////////////////////
        //      Circle      //
        //////////////////////
        if (sData.Circles.IndexCount) {
            uint32_t size = (uint8_t*)sData.Circles.VertexBufferPtr - (uint8_t*)sData.Circles.VertexBufferBase;
            sData.Circles.VertexBuffer->SetData(sData.Circles.VertexBufferBase, size);

            sData.Circles.Shader->Bind();
            RenderCommand::DrawIndexed(sData.Circles.VertexArray.get(), sData.Circles.IndexCount);
        }

        ////////////////////
        //      Line      //
        ////////////////////
        if (sData.Lines.VertexCount) {
            uint32_t data_size = (uint8_t*)sData.Lines.VertexBufferPtr - (uint8_t*)sData.Lines.VertexBufferBase;
            sData.Lines.VertexBuffer->SetData(sData.Lines.VertexBufferBase, data_size);

            sData.Lines.Shader->Bind();
            RenderCommand::DrawLines(sData.Lines.VertexArray.get(), sData.Lines.VertexCount);
        }
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) {
        DrawQuad({ position.x, position.y, 0.0f }, size, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color) {
        auto& q = sData.Quads;
        q.FlushIfFull();

        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

        constexpr size_t    quad_vertex_count = 4;
        constexpr float     texture_index     = 0.0f;
        constexpr glm::vec2 texture_coords[]  = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        constexpr float     tiling_factor     = 1.0f;

        for (size_t i = 0; i < quad_vertex_count; i++) {
            q.VertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            q.VertexBufferPtr->Color        = color;
            q.VertexBufferPtr->TexCoord     = texture_coords[i];
            q.VertexBufferPtr->TexIndex     = texture_index;
            q.VertexBufferPtr->TilingFactor = tiling_factor;
            q.VertexBufferPtr->EntityID     = -1;
            q.VertexBufferPtr++;
        }

        q.IndexCount += 6;
    }

    void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entity_id) {
        auto& q = sData.Quads;
        q.FlushIfFull();

        constexpr size_t    quad_vertex_count = 4;
        constexpr float     texture_index     = 0.0f;
        constexpr glm::vec2 texture_coords[]  = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        constexpr float     tiling_factor     = 1.0f;

        for (size_t i = 0; i < quad_vertex_count; i++) {
            q.VertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            q.VertexBufferPtr->Color        = color;
            q.VertexBufferPtr->TexCoord     = texture_coords[i];
            q.VertexBufferPtr->TexIndex     = texture_index;
            q.VertexBufferPtr->TilingFactor = tiling_factor;
            q.VertexBufferPtr->EntityID     = entity_id;
            q.VertexBufferPtr++;
        }

        q.IndexCount += 6;
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const std::shared_ptr<Texture2D>& texture, const glm::vec4& tint_color) {
        DrawQuad({ position.x, position.y, 0.0f }, size, texture, tint_color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const std::shared_ptr<Texture2D>& texture, const glm::vec4& tint_color) {
        auto& q = sData.Quads;
        q.FlushIfFull();

        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

        float texture_index = 0.0f;
        for (uint32_t i = 1; i < sData.TextureSlotIndex; i++) {
            if (sData.TextureSlots[i].get() == texture.get()) {
                texture_index = (float)i;
                break;
            }
        }

        if (texture_index == 0.0f) {
            texture_index                              = (float)sData.TextureSlotIndex;
            sData.TextureSlots[sData.TextureSlotIndex] = texture;
            sData.TextureSlotIndex++;
        }

        constexpr size_t    quad_vertex_count = 4;
        constexpr glm::vec2 texture_coords[]  = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        constexpr float     tiling_factor     = 1.0f;

        for (size_t i = 0; i < quad_vertex_count; i++) {
            q.VertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            q.VertexBufferPtr->Color        = tint_color;
            q.VertexBufferPtr->TexCoord     = texture_coords[i];
            q.VertexBufferPtr->TexIndex     = texture_index;
            q.VertexBufferPtr->TilingFactor = tiling_factor;
            q.VertexBufferPtr->EntityID     = -1;
            q.VertexBufferPtr++;
        }

        q.IndexCount += 6;
    }

    void Renderer2D::DrawQuad(const glm::mat4& transform, const std::shared_ptr<Texture2D>& texture, const glm::vec4& tint_color, float tiling_factor, int entity_id) {
        auto& q = sData.Quads;
        q.FlushIfFull();

        float texture_index = 0.0f;
        for (uint32_t i = 1; i < sData.TextureSlotIndex; i++) {
            if (sData.TextureSlots[i].get() == texture.get()) {
                texture_index = (float)i;
                break;
            }
        }

        if (texture_index == 0.0f) {
            texture_index                              = (float)sData.TextureSlotIndex;
            sData.TextureSlots[sData.TextureSlotIndex] = texture;
            sData.TextureSlotIndex++;
        }

        constexpr size_t    quad_vertex_count = 4;
        constexpr glm::vec2 texture_coords[]  = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

        for (size_t i = 0; i < quad_vertex_count; i++) {
            q.VertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            q.VertexBufferPtr->Color        = tint_color;
            q.VertexBufferPtr->TexCoord     = texture_coords[i];
            q.VertexBufferPtr->TexIndex     = texture_index;
            q.VertexBufferPtr->TilingFactor = tiling_factor;
            q.VertexBufferPtr->EntityID     = entity_id;
            q.VertexBufferPtr++;
        }

        q.IndexCount += 6;
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const std::shared_ptr<SubTexture2D>& sub_texture, const glm::vec4& tint_color) {
        DrawQuad({ position.x, position.y, 0.0f }, size, sub_texture, tint_color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const std::shared_ptr<SubTexture2D>& sub_texture, const glm::vec4& tint_color) {
        auto& q = sData.Quads;
        q.FlushIfFull();

        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

        const std::shared_ptr<Texture2D> texture           = sub_texture->GetTexture();
        const glm::vec2*                 texture_coords    = sub_texture->GetTexCoords();
        constexpr size_t                 quad_vertex_count = 4;

        float texture_index = 0.0f;
        for (uint32_t i = 1; i < sData.TextureSlotIndex; i++) {
            if (sData.TextureSlots[i].get() == texture.get()) {
                texture_index = (float)i;
                break;
            }
        }

        if (texture_index == 0.0f) {
            texture_index                              = (float)sData.TextureSlotIndex;
            sData.TextureSlots[sData.TextureSlotIndex] = texture;
            sData.TextureSlotIndex++;
        }

        for (int i = 0; i < quad_vertex_count; i++) {
            q.VertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            q.VertexBufferPtr->Color        = tint_color;
            q.VertexBufferPtr->TexCoord     = texture_coords[i];
            q.VertexBufferPtr->TexIndex     = texture_index;
            q.VertexBufferPtr->TilingFactor = 1.0f;
            q.VertexBufferPtr->EntityID     = -1;
            q.VertexBufferPtr++;
        }

        q.IndexCount += 6;
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color) {
        DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color) {
        auto& q = sData.Quads;
        q.FlushIfFull();

        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f }) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

        constexpr size_t    quad_vertex_count = 4;
        constexpr float     texture_index     = 0.0f;
        constexpr glm::vec2 texture_coords[]  = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        constexpr float     tiling_factor     = 1.0f;

        for (size_t i = 0; i < quad_vertex_count; i++) {
            q.VertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            q.VertexBufferPtr->Color        = color;
            q.VertexBufferPtr->TexCoord     = texture_coords[i];
            q.VertexBufferPtr->TexIndex     = texture_index;
            q.VertexBufferPtr->TilingFactor = tiling_factor;
            q.VertexBufferPtr->EntityID     = -1;
            q.VertexBufferPtr++;
        }

        q.IndexCount += 6;
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const std::shared_ptr<Texture2D>& texture, const glm::vec4& tint_color) {
        DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, texture, tint_color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const std::shared_ptr<Texture2D>& texture, const glm::vec4& tint_color) {
        auto& q = sData.Quads;
        q.FlushIfFull();

        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f }) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

        float texture_index = 0.0f;
        for (uint32_t i = 1; i < sData.TextureSlotIndex; i++) {
            if (sData.TextureSlots[i].get() == texture.get()) {
                texture_index = (float)i;
                break;
            }
        }

        if (texture_index == 0.0f) {
            texture_index                              = (float)sData.TextureSlotIndex;
            sData.TextureSlots[sData.TextureSlotIndex] = texture;
            sData.TextureSlotIndex++;
        }

        constexpr size_t    quad_vertex_count = 4;
        constexpr glm::vec2 texture_coords[]  = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        constexpr float     tiling_factor     = 1.0f;

        for (size_t i = 0; i < quad_vertex_count; i++) {
            q.VertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            q.VertexBufferPtr->Color        = tint_color;
            q.VertexBufferPtr->TexCoord     = texture_coords[i];
            q.VertexBufferPtr->TexIndex     = texture_index;
            q.VertexBufferPtr->TilingFactor = tiling_factor;
            q.VertexBufferPtr->EntityID     = -1;
            q.VertexBufferPtr++;
        }

        q.IndexCount += 6;
    }

    void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int entity_id) {
        auto& q = sData.Circles;
        q.FlushIfFull();

        constexpr size_t circle_vertex_count = 4;

        for (size_t i = 0; i < circle_vertex_count; i++) {
            q.VertexBufferPtr->Position      = transform * sData.QuadVertexPositions[i];
            q.VertexBufferPtr->LocalPosition = sData.QuadVertexPositions[i] * 2.0f;
            q.VertexBufferPtr->Color         = color;
            q.VertexBufferPtr->Thickness     = thickness;
            q.VertexBufferPtr->Fade          = fade;
            q.VertexBufferPtr->EntityID      = entity_id;
            q.VertexBufferPtr++;
        }

        q.IndexCount += 6;
    }

    void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entity_id) {
        auto& q = sData.Lines;
        q.FlushIfFull();

        q.VertexBufferPtr->Position = p0;
        q.VertexBufferPtr->Color    = color;
        q.VertexBufferPtr->EntityID = entity_id;
        q.VertexBufferPtr++;

        q.VertexBufferPtr->Position = p1;
        q.VertexBufferPtr->Color    = color;
        q.VertexBufferPtr->EntityID = entity_id;
        q.VertexBufferPtr++;

        q.VertexCount += 2;
    }

    void Renderer2D::NextBatch() {
        Flush();
        ResetBatches();
    }

    void Renderer2D::ResetBatches() {
        sData.Quads.Reset();
        sData.Circles.Reset();
        sData.Lines.Reset();
        sData.TextureSlotIndex = 1;
    }

} // namespace Loom
