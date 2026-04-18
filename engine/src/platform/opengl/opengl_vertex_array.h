#pragma once

#include "loom/renderer/vertex_array.h"

namespace Loom {

    class OpenGLVertexArray : public VertexArray {
    public:
        OpenGLVertexArray();
        ~OpenGLVertexArray() override;

        void Bind() const override;
        void Unbind() const override;

        void AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertex_buffer) override;
        void SetIndexBuffer(const std::shared_ptr<IndexBuffer>& index_buffer) override;

        const std::vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffers() const override { return mVertexBuffers; }
        const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const override { return mIndexBuffer; }

    private:
        uint32_t mRendererID;
        std::vector<std::shared_ptr<VertexBuffer>> mVertexBuffers;
        std::shared_ptr<IndexBuffer> mIndexBuffer;
    };

} // namespace Loom