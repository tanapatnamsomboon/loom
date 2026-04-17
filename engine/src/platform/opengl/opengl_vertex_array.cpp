#include "platform/opengl/opengl_vertex_array.h"
#include "loom/core/log.h"
#include <glad/glad.h>

namespace Loom {

    static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type) {
        switch (type) {
            case ShaderDataType::Float:     return GL_FLOAT;
            case ShaderDataType::Float2:    return GL_FLOAT;
            case ShaderDataType::Float3:    return GL_FLOAT;
            case ShaderDataType::Float4:    return GL_FLOAT;
            case ShaderDataType::Mat3:      return GL_FLOAT;
            case ShaderDataType::Mat4:      return GL_FLOAT;
            case ShaderDataType::Int:       return GL_INT;
            case ShaderDataType::Int2:      return GL_INT;
            case ShaderDataType::Int3:      return GL_INT;
            case ShaderDataType::Int4:      return GL_INT;
            case ShaderDataType::Bool:      return GL_BOOL;
            default: break;
        }
        return GL_NONE;
    }

    OpenGLVertexArray::OpenGLVertexArray() {
        glCreateVertexArrays(1, &mRendererID);
    }

    OpenGLVertexArray::~OpenGLVertexArray() {
        glDeleteVertexArrays(1, &mRendererID);
    }

    void OpenGLVertexArray::Bind() const { glBindVertexArray(mRendererID); }
    void OpenGLVertexArray::Unbind() const { glBindVertexArray(0); }

    void OpenGLVertexArray::AddVertexBuffer(VertexBuffer* vertex_buffer) {
        if (vertex_buffer->GetLayout().GetElements().empty()) {
            LOOM_CORE_FATAL("Vertex Buffer has no layout!");
        }

        glBindVertexArray(mRendererID);
        vertex_buffer->Bind();

        uint32_t index = 0;
        const auto& layout = vertex_buffer->GetLayout();
        for (const auto& element : layout) {
            glEnableVertexAttribArray(index);
            glVertexAttribPointer(
                index,
                element.GetComponentCount(),
                ShaderDataTypeToOpenGLBaseType(element.Type),
                element.Normalized ? GL_TRUE : GL_FALSE,
                layout.GetStride(),
                (const void*)(intptr_t)element.Offset
            );
            index++;
        }

        mVertexBuffers.push_back(vertex_buffer);
    }

    void OpenGLVertexArray::SetIndexBuffer(IndexBuffer* index_buffer) {
        glBindVertexArray(mRendererID);
        index_buffer->Bind();
        mIndexBuffer = index_buffer;
    }

} // namespace Loom