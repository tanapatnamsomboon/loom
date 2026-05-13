#include "loom/renderer/renderer_2d.h"
#include "loom/asset/asset_manager.h"
#include "loom/project/project.h"
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

        std::shared_ptr<VertexArray>  VAO;
        std::shared_ptr<VertexBuffer> VBO;
        std::shared_ptr<Shader>       ActiveShader;

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

        // Quad Setup
        auto& quad = sData.Quads;
        quad.VAO = VertexArray::Create();
        quad.VBO = VertexBuffer::Create(sData.MaxVertices * sizeof(QuadVertex));
        quad.VBO->SetLayout({ { ShaderDataType::Float3, "aPosition" },
                                    { ShaderDataType::Float4, "aColor" },
                                    { ShaderDataType::Float2, "aTexCoord" },
                                    { ShaderDataType::Float, "aTexIndex" },
                                    { ShaderDataType::Float, "aTilingFactor" },
                                    { ShaderDataType::Int, "aEntityID" } });
        quad.VAO->AddVertexBuffer(quad.VBO);
        quad.VAO->SetIndexBuffer(ibo);
        quad.VertexBufferBase = new QuadVertex[sData.MaxVertices];
        std::string quad_shader_path = Project::GetEngineAssetFileSystemPath("shaders/quad").generic_string();
        quad.ActiveShader            = AssetManager::GetShader(quad_shader_path);

        int32_t samplers[sData.MaxTextureSlots];
        for (uint32_t i = 0; i < sData.MaxTextureSlots; i++)
            samplers[i] = i;
        quad.ActiveShader->Bind();
        quad.ActiveShader->UploadUniformIntArray("uTextures", samplers, sData.MaxTextureSlots);

        // White texture
        sData.WhiteTexture = Texture2D::Create(1, 1);
        uint32_t white     = 0xFFFFFFFF;
        sData.WhiteTexture->SetData(&white, sizeof(uint32_t));
        sData.TextureSlots[0] = sData.WhiteTexture;

        // Circle Setup
        auto& circle = sData.Circles;
        circle.VAO = VertexArray::Create();
        circle.VBO = VertexBuffer::Create(sData.MaxVertices * sizeof(CircleVertex));
        circle.VBO->SetLayout({ { ShaderDataType::Float3, "aPosition" },
                                    { ShaderDataType::Float3, "aLocalPosition" },
                                    { ShaderDataType::Float4, "aColor" },
                                    { ShaderDataType::Float, "aThickness" },
                                    { ShaderDataType::Float, "aFade" },
                                    { ShaderDataType::Int, "aEntityID" } });
        circle.VAO->AddVertexBuffer(circle.VBO);
        circle.VAO->SetIndexBuffer(ibo);
        circle.VertexBufferBase = new CircleVertex[sData.MaxVertices];

        std::string circle_shader_path = Project::GetEngineAssetFileSystemPath("shaders/circle").generic_string();
        circle.ActiveShader            = AssetManager::GetShader(circle_shader_path);

        // Line Setup
        auto& line = sData.Lines;
        line.VAO = VertexArray::Create();
        line.VBO = VertexBuffer::Create(sData.MaxVertices * sizeof(LineVertex));
        line.VBO->SetLayout({ { ShaderDataType::Float3, "aPosition" },
                                    { ShaderDataType::Float4, "aColor" },
                                    { ShaderDataType::Int, "aEntityID" } });
        line.VAO->AddVertexBuffer(line.VBO);
        line.VertexBufferBase = new LineVertex[sData.MaxVertices];

        std::string line_shader_path = Project::GetEngineAssetFileSystemPath("shaders/line").generic_string();
        line.ActiveShader            = AssetManager::GetShader(line_shader_path);


        sData.Quads.SetFlushCallback([]() { NextBatch(); });
        sData.Circles.SetFlushCallback([]() { NextBatch(); });
        sData.Lines.SetFlushCallback([]() { NextBatch(); });

        sData.CameraUniformBuffer = UniformBuffer::Create(sizeof(glm::mat4), 0);
    }

    void Renderer2D::Shutdown() {
        delete[] sData.Quads.VertexBufferBase;
        delete[] sData.Circles.VertexBufferBase;
        delete[] sData.Lines.VertexBufferBase;
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
        // Quads
        if (sData.Quads.IndexCount) {
            uint32_t size = (uint8_t*)sData.Quads.VertexBufferPtr - (uint8_t*)sData.Quads.VertexBufferBase;
            sData.Quads.VBO->SetData(sData.Quads.VertexBufferBase, size);

            for (uint32_t i = 0; i < sData.TextureSlotIndex; ++i)
                sData.TextureSlots[i]->Bind(i);

            sData.Quads.ActiveShader->Bind();
            RenderCommand::DrawIndexed(sData.Quads.VAO.get(), sData.Quads.IndexCount);
        }

        // Circle
        if (sData.Circles.IndexCount) {
            uint32_t size = (uint8_t*)sData.Circles.VertexBufferPtr - (uint8_t*)sData.Circles.VertexBufferBase;
            sData.Circles.VBO->SetData(sData.Circles.VertexBufferBase, size);

            sData.Circles.ActiveShader->Bind();
            RenderCommand::DrawIndexed(sData.Circles.VAO.get(), sData.Circles.IndexCount);
        }

        // Line
        if (sData.Lines.VertexCount) {
            uint32_t data_size = (uint8_t*)sData.Lines.VertexBufferPtr - (uint8_t*)sData.Lines.VertexBufferBase;
            sData.Lines.VBO->SetData(sData.Lines.VertexBufferBase, data_size);

            sData.Lines.ActiveShader->Bind();
            RenderCommand::DrawLines(sData.Lines.VAO.get(), sData.Lines.VertexCount);
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
            if (sData.TextureSlotIndex >= Renderer2DStorage::MaxTextureSlots)
                NextBatch();

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
            if (sData.TextureSlotIndex >= Renderer2DStorage::MaxTextureSlots)
                NextBatch();

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

    void Renderer2D::DrawQuad(const glm::mat4& transform, const std::shared_ptr<Texture2D>& texture, const glm::vec2 tex_coords[4], const glm::vec4& tint_color, int entity_id) {
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
            if (sData.TextureSlotIndex >= Renderer2DStorage::MaxTextureSlots)
                NextBatch();

            texture_index                              = (float)sData.TextureSlotIndex;
            sData.TextureSlots[sData.TextureSlotIndex] = texture;
            sData.TextureSlotIndex++;
        }

        constexpr size_t quad_vertex_count = 4;
        for (size_t i = 0; i < quad_vertex_count; i++) {
            q.VertexBufferPtr->Position     = transform * sData.QuadVertexPositions[i];
            q.VertexBufferPtr->Color        = tint_color;
            q.VertexBufferPtr->TexCoord     = tex_coords[i];
            q.VertexBufferPtr->TexIndex     = texture_index;
            q.VertexBufferPtr->TilingFactor = 1.0f;
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
            if (sData.TextureSlotIndex >= Renderer2DStorage::MaxTextureSlots)
                NextBatch();

            texture_index                              = (float)sData.TextureSlotIndex;
            sData.TextureSlots[sData.TextureSlotIndex] = texture;
            sData.TextureSlotIndex++;
        }

        for (size_t i = 0; i < quad_vertex_count; i++) {
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

    void Renderer2D::DrawText(const std::string& text, const std::shared_ptr<FontAsset>& font,
                              const glm::mat4& transform, const glm::vec4& color,
                              float kerning, float line_spacing, int entity_id) {
        if (!font || text.empty()) return;
        auto atlas = font->GetAtlasTexture();
        if (!atlas) return;

        float cursor_x = 0.0f;
        float cursor_y = 0.0f;

        for (char c : text) {
            if (c == '\n') {
                cursor_x  = 0.0f;
                cursor_y -= font->GetLineHeight() + line_spacing;
                continue;
            }

            GlyphData g;
            if (!font->GetGlyphData(c, g)) {
                cursor_x += kerning;
                continue;
            }

            float quad_cx = cursor_x + (g.QuadMin.x + g.QuadMax.x) * 0.5f;
            float quad_cy = cursor_y + (g.QuadMin.y + g.QuadMax.y) * 0.5f;
            float quad_w  = g.QuadMax.x - g.QuadMin.x;
            float quad_h  = g.QuadMax.y - g.QuadMin.y;

            glm::mat4 glyph_transform = transform
                * glm::translate(glm::mat4(1.0f), { quad_cx, quad_cy, 0.0f })
                * glm::scale(glm::mat4(1.0f), { quad_w, quad_h, 1.0f });

            // Atlas is uploaded without a V-flip, so stb's t0 (top row) sits at a low
            // GL v-value and t1 (bottom row) at a higher one. Assign t1 to the bottom
            // quad vertices and t0 to the top so the glyph renders right-side up.
            const glm::vec2 tex_coords[4] = {
                { g.s0, g.t1 },  // BL
                { g.s1, g.t1 },  // BR
                { g.s1, g.t0 },  // TR
                { g.s0, g.t0 },  // TL
            };

            DrawQuad(glyph_transform, atlas, tex_coords, color, entity_id);

            cursor_x += g.Advance + kerning;
        }
    }

    void Renderer2D::DrawTilemap(const std::shared_ptr<Texture2D>& spritesheet,
                                 const glm::mat4& transform,
                                 int columns, int rows,
                                 float tile_width, float tile_height,
                                 int sheet_columns, int sheet_rows,
                                 const std::vector<int>& tiles,
                                 int entity_id) {
        if (!spritesheet || columns <= 0 || rows <= 0 || sheet_columns <= 0 || sheet_rows <= 0) return;
        if ((int)tiles.size() != columns * rows) return;

        const float inv_sc = 1.0f / (float)sheet_columns;
        const float inv_sr = 1.0f / (float)sheet_rows;

        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < columns; ++col) {
                int tile_idx = tiles[row * columns + col];
                if (tile_idx < 0) continue;

                float x_c = (col + 0.5f - columns * 0.5f) * tile_width;
                float y_c = (rows * 0.5f - row - 0.5f) * tile_height;

                int sheet_col = tile_idx % sheet_columns;
                int sheet_row = tile_idx / sheet_columns;

                float u0 = (float)sheet_col * inv_sc;
                float u1 = (float)(sheet_col + 1) * inv_sc;
                float v0 = 1.0f - (float)(sheet_row + 1) * inv_sr; // GL bottom
                float v1 = 1.0f - (float)sheet_row * inv_sr;        // GL top

                const glm::vec2 tex_coords[4] = {
                    { u0, v0 }, { u1, v0 }, { u1, v1 }, { u0, v1 }
                };

                glm::mat4 tile_transform = transform
                    * glm::translate(glm::mat4(1.0f), { x_c, y_c, 0.0f })
                    * glm::scale(glm::mat4(1.0f), { tile_width, tile_height, 1.0f });

                DrawQuad(tile_transform, spritesheet, tex_coords, glm::vec4(1.0f), entity_id);
            }
        }
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
            if (sData.TextureSlotIndex >= Renderer2DStorage::MaxTextureSlots)
                NextBatch();

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
        q.FlushIfFull(2);

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
