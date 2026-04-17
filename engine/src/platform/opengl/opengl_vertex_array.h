#pragma once

#include "loom/renderer/vertex_array.h"

namespace Loom {

    class OpenGLVertexArray : public VertexArray {
    public:
        OpenGLVertexArray();
        ~OpenGLVertexArray() override;

        void Bind() const override;
        void Unbind() const override;

        void AddVertexBuffer(VertexBuffer* vertex_buffer) override;
        void SetIndexBuffer(IndexBuffer* index_buffer) override;

        const std::vector<VertexBuffer*>& GetVertexBuffers() const override { return mVertexBuffers; }
        IndexBuffer* GetIndexBuffer() const override { return mIndexBuffer; }

    private:
        uint32_t mRendererID;
        std::vector<VertexBuffer*> mVertexBuffers;
        IndexBuffer* mIndexBuffer;
    };

} // namespace Loom