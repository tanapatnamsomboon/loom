#include <loom/core/application.h>
#include <loom/core/log.h>
#include <loom/core/layer.h>
#include <loom/renderer/render_command.h>
#include <loom/renderer/vertex_array.h>
#include <loom/renderer/buffer.h>
#include <loom/renderer/shader.h>
#include <imgui.h>

class ExampleLayer : public Loom::Layer {
public:
    ExampleLayer() : Layer("Example") {
        mVertexArray.reset(Loom::VertexArray::Create());

        float vertices[3 * 3] = {
            -0.5f, -0.5f,  0.0f,
             0.5f, -0.5f,  0.0f,
             0.0f,  0.5f,  0.0f
        };

        mVertexBuffer.reset(Loom::VertexBuffer::Create(vertices, sizeof(vertices)));

        Loom::BufferLayout layout = {
            { Loom::ShaderDataType::Float3, "aPosition" }
        };
        mVertexBuffer->SetLayout(layout);
        mVertexArray->AddVertexBuffer(mVertexBuffer.get());

        uint32_t indices[3] = { 0, 1, 2 };
        mIndexBuffer.reset(Loom::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
        mVertexArray->SetIndexBuffer(mIndexBuffer.get());

        std::string vertex_src = R"(
            #version 460 core
            layout(location = 0) in vec3 aPosition;

            void main() {
                gl_Position = vec4(aPosition, 1.0);
            }
        )";

        std::string fragment_src = R"(
            #version 460 core
            layout(location = 0) out vec4 color;

            void main() {
                color = vec4(0.8, 0.2, 0.3, 1.0);
            }
        )";

        mShader.reset(Loom::Shader::Create(vertex_src, fragment_src));
    }

    void OnAttach() override {
        ImGuiContext* context;
        ImGuiMemAllocFunc alloc_func;
        ImGuiMemFreeFunc free_func;
        void* user_data;

        Loom::Application::Get().GetImGuiLayer()->GetContextAndAllocators(&context, &alloc_func, &free_func, &user_data);

        ImGui::SetCurrentContext(context);
        ImGui::SetAllocatorFunctions(alloc_func, free_func, user_data);
    }

    void OnUpdate() override {
        Loom::RenderCommand::SetClearColor(0.2f, 0.2f, 0.8f, 1.0f);
        Loom::RenderCommand::Clear();

        mShader->Bind();
        mVertexArray->Bind();
        Loom::RenderCommand::DrawIndexed(mVertexArray.get());
    }

    void OnImGuiRender() override {
        ImGui::Begin("Settings");
        ImGui::Text("Hello Loom Engine!");
        ImGui::End();
    }

private:
    std::shared_ptr<Loom::Shader> mShader;
    std::shared_ptr<Loom::VertexArray> mVertexArray;
    std::shared_ptr<Loom::VertexBuffer> mVertexBuffer;
    std::shared_ptr<Loom::IndexBuffer> mIndexBuffer;
};

int main() {
    Loom::Log::Init();
    LOOM_INFO("Sandbox initialized!");

    Loom::Application app;
    app.PushLayer(new ExampleLayer());
    app.Run();
    return 0;
}