#pragma once

#include "loom/renderer/buffer.h"

namespace Loom {

    class OpenGLVertexBuffer : public VertexBuffer {
    public:
        OpenGLVertexBuffer(float* vertices, uint32_t size);
        ~OpenGLVertexBuffer() override;

        void Bind() const override;
        void Unbind() const override;

    private:
        uint32_t mRendererID;
    };

    class OpenGLIndexBuffer : public IndexBuffer {
    public:
        OpenGLIndexBuffer(uint32_t* indices, uint32_t count);
        ~OpenGLIndexBuffer() override;

        void Bind() const override;
        void Unbind() const override;
        uint32_t GetCount() const override { return mCount; }

    private:
        uint32_t mRendererID;
        uint32_t mCount;
    };

} // namespace Loom