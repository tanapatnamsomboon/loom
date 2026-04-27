#pragma once

#include "loom/core/core.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace Loom {

    class LOOM_API UniformBuffer {
    public:
        virtual ~UniformBuffer() = default;
        virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;

        static std::shared_ptr<UniformBuffer> Create(uint32_t size, uint32_t binding);
    };

    enum class ShaderDataType {
        None = 0, Float, Float2, Float3, Float4, Mat3, Mat4, Int, Int2, Int3, Int4, Bool
    };

    inline uint32_t ShaderDataTypeSize(ShaderDataType type) {
        switch (type) {
            case ShaderDataType::Float:     return 4;
            case ShaderDataType::Float2:    return 4 * 2;
            case ShaderDataType::Float3:    return 4 * 3;
            case ShaderDataType::Float4:    return 4 * 4;
            case ShaderDataType::Mat3:      return 4 * 3 * 3;
            case ShaderDataType::Mat4:      return 4 * 4 * 4;
            case ShaderDataType::Int:       return 4;
            case ShaderDataType::Int2:      return 4 * 2;
            case ShaderDataType::Int3:      return 4 * 3;
            case ShaderDataType::Int4:      return 4 * 4;
            case ShaderDataType::Bool:      return 1;
            default: break;
        }
        return 0;
    }

    struct BufferElement {
        std::string Name;
        ShaderDataType Type;
        uint32_t Size;
        uint32_t Offset;
        bool Normalized;

        BufferElement() = default;
        BufferElement(ShaderDataType type, const std::string& name, bool normalized = false)
            : Name(name), Type(type), Size(ShaderDataTypeSize(type)), Offset(0), Normalized(normalized) {}

        uint32_t GetComponentCount() const {
            switch (Type) {
                case ShaderDataType::Float:     return 1;
                case ShaderDataType::Float2:    return 2;
                case ShaderDataType::Float3:    return 3;
                case ShaderDataType::Float4:    return 4;
                case ShaderDataType::Mat3:      return 3 * 3;
                case ShaderDataType::Mat4:      return 4 * 4;
                case ShaderDataType::Int:       return 1;
                case ShaderDataType::Int2:      return 2;
                case ShaderDataType::Int3:      return 3;
                case ShaderDataType::Int4:      return 4;
                case ShaderDataType::Bool:      return 1;
                default: break;
            }
            return 0;
        }
    };

    class BufferLayout {
    public:
        BufferLayout() {}
        BufferLayout(const std::initializer_list<BufferElement>& elements)
            : mElements(elements) {
            CalculateOffsetsAndStride();
        }

        uint32_t GetStride() const { return mStride; }
        const std::vector<BufferElement>& GetElements() const { return mElements; }

        std::vector<BufferElement>::iterator begin() { return mElements.begin(); }
        std::vector<BufferElement>::iterator end() { return mElements.end(); }
        std::vector<BufferElement>::const_iterator begin() const { return mElements.begin(); }
        std::vector<BufferElement>::const_iterator end() const { return mElements.end(); }

    private:
        void CalculateOffsetsAndStride() {
            uint32_t offset = 0;
            mStride = 0;
            for (auto& element : mElements) {
                element.Offset = offset;
                offset += element.Size;
                mStride += element.Size;
            }
        }

    private:
        std::vector<BufferElement> mElements;
        uint32_t mStride = 0;
    };

    class LOOM_API VertexBuffer {
    public:
        virtual ~VertexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void SetData(const void* data, uint32_t size) = 0;

        virtual const BufferLayout& GetLayout() const = 0;
        virtual void SetLayout(const BufferLayout& layout) = 0;

        static std::shared_ptr<VertexBuffer> Create(uint32_t size);
    };

    class LOOM_API IndexBuffer {
    public:
        virtual ~IndexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual uint32_t GetCount() const = 0;

        static std::shared_ptr<IndexBuffer> Create(uint32_t* indices, uint32_t count);
    };

} // namespace Loom