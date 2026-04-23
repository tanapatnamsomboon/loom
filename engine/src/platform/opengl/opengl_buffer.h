#pragma once

#include "loom/renderer/buffer.h"

namespace Loom {

    class OpenGLUniformBuffer : public UniformBuffer {
    public:
        OpenGLUniformBuffer(uint32_t size, uint32_t binding);
        ~OpenGLUniformBuffer() override;

        void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;

    private:
        uint32_t mRendererID = 0;
    };

    class OpenGLVertexBuffer : public VertexBuffer {
    public:
        OpenGLVertexBuffer(uint32_t size);
        ~OpenGLVertexBuffer() override;

        void Bind() const override;
        void Unbind() const override;

        void SetData(const void* data, uint32_t size) override;

        const BufferLayout& GetLayout() const override { return mLayout; }
        void SetLayout(const BufferLayout& layout) override { mLayout = layout; }

    private:
        uint32_t mRendererID;
        BufferLayout mLayout;
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