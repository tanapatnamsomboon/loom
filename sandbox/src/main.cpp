#include <loom/core/application.h>
#include <loom/core/input.h>
#include <loom/core/log.h>
#include <loom/core/layer.h>
#include <loom/renderer/buffer.h>
#include <loom/renderer/orthographic_camera.h>
#include <loom/renderer/render_command.h>
#include <loom/renderer/shader.h>
#include <loom/renderer/vertex_array.h>
#include <imgui.h>

class ExampleLayer : public Loom::Layer {
public:
    ExampleLayer()
        : Layer("Example"), mCamera(-1.6f, 1.6f, -0.9f, 0.9f), mCameraPosition(0.0f) {
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

            uniform mat4 uViewProjection;

            void main() {
                gl_Position = uViewProjection * vec4(aPosition, 1.0);
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

    void OnUpdate(Loom::Timestep ts) override {
        if (Loom::Input::IsKeyPressed('A'))
            mCameraPosition.x -= mCameraMoveSpeed * ts;
        else if (Loom::Input::IsKeyPressed('D'))
            mCameraPosition.x += mCameraMoveSpeed * ts;

        if (Loom::Input::IsKeyPressed('W'))
            mCameraPosition.y += mCameraMoveSpeed * ts;
        else if (Loom::Input::IsKeyPressed('S'))
            mCameraPosition.y -= mCameraMoveSpeed * ts;

        mCamera.SetPosition(mCameraPosition);

        Loom::RenderCommand::SetClearColor(0.2f, 0.2f, 0.8f, 1.0f);
        Loom::RenderCommand::Clear();

        mShader->Bind();
        mShader->UploadUniformMat4("uViewProjection", mCamera.GetViewProjectionMatrix());
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

    Loom::OrthographicCamera mCamera;
    glm::vec3 mCameraPosition;
    float mCameraMoveSpeed = 1.0f;
};

int main() {
    Loom::Log::Init();
    LOOM_INFO("Sandbox initialized!");

    Loom::Application app;
    app.PushLayer(new ExampleLayer());
    app.Run();
    return 0;
}