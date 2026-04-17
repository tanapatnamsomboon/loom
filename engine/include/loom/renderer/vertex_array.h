#pragma once

#include "loom/core/core.h"
#include "loom/renderer/buffer.h"

namespace Loom {

    class LOOM_API VertexArray {
    public:
        virtual ~VertexArray() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void AddVertexBuffer(VertexBuffer* vertex_buffer) = 0;
        virtual void SetIndexBuffer(IndexBuffer* index_buffer) = 0;

        virtual const std::vector<VertexBuffer*>& GetVertexBuffers() const = 0;
        virtual IndexBuffer* GetIndexBuffer() const = 0;

        static VertexArray* Create();
    };

} // namespace Loom