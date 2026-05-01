#include "editor_layer.h"
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
// clang-format off
#include <ImGuizmo.h>
#include <imgui_internal.h>
// clang-format on
#include <loom/asset/asset_manager.h>
#include <loom/core/application.h>
#include <loom/core/input.h>
#include <loom/math/math.h>
#include <loom/project/project_serializer.h>
#include <loom/renderer/framebuffer.h>
#include <loom/renderer/render_command.h>
#include <loom/renderer/renderer_2d.h>
#include <loom/scene/components.h>
#include <loom/scene/scene_serializer.h>
#include <nfd.hpp>
#include <filesystem>

namespace Weaver {

#pragma region Construction & Initialization

    EditorLayer::EditorLayer()
        : Layer("EditorLayer") {
        // Initialize Framebuffer
        Loom::FramebufferSpecification fb_spec;
        fb_spec.Attachments = {
            Loom::FramebufferTextureFormat::RGBA8,
            Loom::FramebufferTextureFormat::RED_INTEGER,
            Loom::FramebufferTextureFormat::DEPTH24STENCIL8
        };
        fb_spec.Width  = 1280;
        fb_spec.Height = 720;
        mFramebuffer   = Loom::Framebuffer::Create(fb_spec);

        // Scene setup
        mEditorScene = std::make_shared<Loom::Scene>();
        mActiveScene = mEditorScene;

        // Camera
        mEditorCamera = Loom::EditorCamera(30.0f, 1.778f, 0.1f, 1000.0f);

        // Hierarchy Panel
        mSceneHierarchyPanel.SetContext(mActiveScene);
    }

    void EditorLayer::OnAttach() {
        mShowProjectWizard = true;

        std::string skybox_path = Loom::Project::GetEngineAssetFileSystemPath("shaders/skybox").generic_string();
        std::string grid_path = Loom::Project::GetEngineAssetFileSystemPath("shaders/grid").generic_string();

        mContentBrowserPanel.Init();

        ImGuiContext*     context;
        ImGuiMemAllocFunc alloc_func;
        ImGuiMemFreeFunc  free_func;
        void*             user_data;

        Loom::Application::Get().GetImGuiLayer()->GetContextAndAllocators(&context, &alloc_func, &free_func, &user_data);

        ImGui::SetCurrentContext(context);
        ImGuizmo::SetImGuiContext(context);
        ImGui::SetAllocatorFunctions(alloc_func, free_func, user_data);

        float skybox_vertices[] = {
            -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f
        };

        // Skybox Initialization
        uint32_t skybox_indices[] = {
            1, 2, 6, 6, 5, 1, // Right
            0, 4, 7, 7, 3, 0, // Left
            3, 7, 6, 6, 2, 3, // Top
            0, 1, 5, 5, 4, 0, // Bottom
            5, 6, 7, 7, 4, 5, // Back
            1, 0, 3, 3, 2, 1  // Front
        };

        mSkyboxVAO = Loom::VertexArray::Create();

        mSkyboxVBO = Loom::VertexBuffer::Create(sizeof(skybox_vertices));
        mSkyboxVBO->SetData(skybox_vertices, sizeof(skybox_vertices));
        mSkyboxVBO->SetLayout({ { Loom::ShaderDataType::Float3, "aPosition" } });
        mSkyboxVAO->AddVertexBuffer(mSkyboxVBO);

        auto skybox_ibo = Loom::IndexBuffer::Create(skybox_indices, sizeof(skybox_indices) / sizeof(uint32_t));
        mSkyboxVAO->SetIndexBuffer(skybox_ibo);

        mSkyboxShader = Loom::AssetManager::GetShader(skybox_path);

        // Grid Initialization
        float grid_vertices[] = {
            -1.0f, 0.0f, -1.0f,
             1.0f, 0.0f, -1.0f,
             1.0f, 0.0f,  1.0f,
            -1.0f, 0.0f,  1.0f
        };

        uint32_t grid_indices[] = { 0, 1, 2, 2, 3, 0 };

        mGridVAO = Loom::VertexArray::Create();

        mGridVBO = Loom::VertexBuffer::Create(sizeof(grid_vertices));
        mGridVBO->SetData(grid_vertices, sizeof(grid_vertices));
        mGridVBO->SetLayout({ { Loom::ShaderDataType::Float3, "aPosition" } });
        mGridVAO->AddVertexBuffer(mGridVBO);

        auto grid_ibo = Loom::IndexBuffer::Create(grid_indices, sizeof(grid_indices) / sizeof(uint32_t));
        mGridVAO->SetIndexBuffer(grid_ibo);

        mGridShader = Loom::AssetManager::GetShader(grid_path);

        mSceneHierarchyPanel.Init();
    }

#pragma endregion

#pragma region Update Loop

    void EditorLayer::OnUpdate(Loom::Timestep ts) {
        HandleViewportResize();

        mFramebuffer->Bind();
        Loom::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        Loom::RenderCommand::Clear();
        mFramebuffer->ClearAttachment(1, -1);

        UpdateScene(ts);

        HandleMousePicking();

        mFramebuffer->Unbind();
    }

    void EditorLayer::HandleViewportResize() {
        mActiveScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);

        Loom::FramebufferSpecification spec = mFramebuffer->GetSpecification();
        if (mViewportSize.x > 0.0f && mViewportSize.y > 0.0f && (spec.Width != mViewportSize.x || spec.Height != mViewportSize.y)) {
            mFramebuffer->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
            mEditorCamera.SetViewportSize(mViewportSize.x, mViewportSize.y);
        }
    }

    void EditorLayer::UpdateScene(Loom::Timestep ts) {
        if (!Loom::Project::GetActive()) {
            return;
        }

        if (mSceneState == SceneState::Edit) {
            if (mViewportHovered) {
                mEditorCamera.OnUpdate(ts);
            } else {
                mEditorCamera.ResetMousePosition();
            }
        }

        if (mSceneState == SceneState::Edit) {
            // Render Skybox
            glm::mat4 view = mEditorCamera.GetViewMatrix();
            view[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            glm::mat4 skybox_view_projection = mEditorCamera.GetProjectionMatrix() * view;

            mSkyboxShader->Bind();
            mSkyboxShader->UploadUniformMat4("uViewProjection", skybox_view_projection);
            Loom::RenderCommand::DrawIndexed(mSkyboxVAO.get(), 36);

            // Render Grid
            glm::vec3 camera_pos = mEditorCamera.GetPosition();

            glm::mat4 grid_transform = glm::translate(glm::mat4(1.0f), { camera_pos.x, 0.0f, camera_pos.z }) * glm::scale(glm::mat4(1.0f), { 150.0f, 1.0f, 150.0f });

            mGridShader->Bind();
            mGridShader->UploadUniformMat4("uViewProjection", mEditorCamera.GetViewProjectionMatrix());
            mGridShader->UploadUniformMat4("uTransform", grid_transform);
            mGridShader->UploadUniformFloat3("uCameraPosition", camera_pos);

            Loom::RenderCommand::DrawIndexed(mGridVAO.get(), 6);
        }

        switch (mSceneState) {
            case SceneState::Edit:
                mActiveScene->OnUpdateEditor(ts, mEditorCamera, mSceneHierarchyPanel.GetSelectedEntity());
                break;
            case SceneState::Play:
                mActiveScene->OnUpdateRuntime(ts);
                break;
        }
    }

    void EditorLayer::HandleMousePicking() {
        auto [mx, my] = ImGui::GetMousePos();
        mx -= mViewportBounds[0].x;
        my -= mViewportBounds[0].y;

        glm::vec2 viewport_size = mViewportBounds[1] - mViewportBounds[0];
        my                      = viewport_size.y - my;

        int mouse_x = (int)mx;
        int mouse_y = (int)my;

        if (mouse_x >= 0 && mouse_y >= 0 && mouse_x < (int)viewport_size.x && mouse_y < (int)viewport_size.y) {
            int pixel_data = mFramebuffer->ReadPixel(1, mouse_x, mouse_y);
            if (pixel_data == -1) {
                mHoveredEntity = Loom::Entity();
            } else {
                mHoveredEntity = Loom::Entity((entt::entity)pixel_data, mActiveScene.get());
            }
        } else {
            mHoveredEntity = Loom::Entity();
        }
    }

#pragma endregion

#pragma region Input & Events

    void EditorLayer::OnEvent(Loom::Event& event) {
        mEditorCamera.OnEvent(event);

        Loom::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Loom::MouseButtonPressedEvent>(LOOM_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
        dispatcher.Dispatch<Loom::KeyPressedEvent>(LOOM_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
    }

    bool EditorLayer::OnMouseButtonPressed(Loom::MouseButtonPressedEvent& event) {
        if (event.GetMouseButton() == 0 && mViewportHovered && !ImGuizmo::IsOver()) {
            mSceneHierarchyPanel.SetSelectedEntity(mHoveredEntity);
        }
        return false;
    }

    bool EditorLayer::OnKeyPressed(Loom::KeyPressedEvent& event) {
        HandleShortcuts(event);
        HandleGizmoTypeChange(event);
        return false;
    }

    void EditorLayer::HandleShortcuts(Loom::KeyPressedEvent& event) {
        bool ctrl  = Loom::Input::IsKeyPressed(Loom::Key::LeftControl) || Loom::Input::IsKeyPressed(Loom::Key::RightControl);
        bool shift = Loom::Input::IsKeyPressed(Loom::Key::LeftShift) || Loom::Input::IsKeyPressed(Loom::Key::RightShift);

        switch ((Loom::Key)event.GetKeyCode()) {
            case Loom::Key::N:
                if (ctrl) {
                    NewScene();
                    return;
                }
                break;
            case Loom::Key::O:
                if (ctrl) {
                    OpenScene();
                    return;
                }
                break;
            case Loom::Key::S:
                if (ctrl && shift) {
                    SaveSceneAs();
                    return;
                }
                if (ctrl) {
                    SaveScene();
                    return;
                }
                break;
            default:
                break;
        }
    }

    void EditorLayer::HandleGizmoTypeChange(Loom::KeyPressedEvent& event) {
        if (Loom::Input::IsMouseButtonPressed(Loom::Mouse::ButtonRight))
            return;

        switch ((Loom::Key)event.GetKeyCode()) {
            case Loom::Key::Q:
                mGizmoType = -1;
                break;
            case Loom::Key::W:
                mGizmoType = ImGuizmo::OPERATION::TRANSLATE;
                break;
            case Loom::Key::E:
                mGizmoType = ImGuizmo::OPERATION::ROTATE;
                break;
            case Loom::Key::R:
                mGizmoType = ImGuizmo::OPERATION::SCALE;
                break;
            default:
                break;
        }
    }

#pragma endregion

#pragma region ImGui Rendering

    void EditorLayer::OnImGuiRender() {
        ImGuizmo::BeginFrame();

        static bool dockspace_open = true;
        static bool opt_fullscreen = true;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

        if (opt_fullscreen) {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("Weaver Main Dockspace", &dockspace_open, window_flags);
        ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("WeaverDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        RenderMainMenuBar();
        RenderModals();
        RenderProjectWizard();
        RenderPanels();
        RenderViewport();

        ImGui::End();
    }

    void EditorLayer::RenderMainMenuBar() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                // Project Management
                if (ImGui::MenuItem("New Project...")) {
                    NewProject();
                }
                if (ImGui::MenuItem("Open Project...")) {
                    OpenProject();
                }
                if (ImGui::MenuItem("Save Project As...")) {
                    SaveProjectAs();
                }

                ImGui::Separator();

                // Scene Management
                if (ImGui::MenuItem("New", "Ctrl+N")) {
                    NewScene();
                }
                if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                    OpenScene();
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    SaveScene();
                }
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                    SaveSceneAs();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Scene Hierarchy", nullptr, &mShowSceneHierarchyPanel);
                ImGui::MenuItem("Content Browser", nullptr, &mShowContentBrowserPanel);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("About")) {
                if (ImGui::MenuItem("About Weaver")) {
                    mShowAboutModal = true;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void EditorLayer::RenderModals() {
        if (mShowAboutModal) {
            ImGui::OpenPopup("About Weaver");
            mShowAboutModal = false;
        }

        if (ImGui::BeginPopupModal("About Weaver", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Weaver Editor");
            ImGui::Separator();
            ImGui::Text("A custom 2D/3D engine editor.");

            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void EditorLayer::RenderPanels() {
        if (mShowSceneHierarchyPanel) {
            mSceneHierarchyPanel.OnImGuiRender();
        }
        if (mShowContentBrowserPanel) {
            mContentBrowserPanel.OnImGuiRender();
        }
    }

    void EditorLayer::RenderToolbar() {
        ImVec2 content_min = ImGui::GetWindowContentRegionMin();
        ImVec2 content_max = ImGui::GetWindowContentRegionMax();

        float button_width = 60.0f;
        float button_height = 28.0f;
        float y_offset = 10.0f;

        float cursor_x = content_min.x + (content_max.x - content_min.x) * 0.5f - (button_width * 0.5f);
        float cursor_y = content_min.y + y_offset;

        ImGui::SetCursorPos(ImVec2(cursor_x, cursor_y));

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, button_height * 0.2f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));

        bool has_active_scene = Loom::Project::GetActive() != nullptr;

        if (!has_active_scene) {
            ImGui::BeginDisabled();
        }

        if (mSceneState == SceneState::Edit) {
            if (ImGui::Button("Play", ImVec2(button_width, button_height)))
                OnScenePlay();
        } else if (mSceneState == SceneState::Play) {
            if (ImGui::Button("Stop", ImVec2(button_width, button_height)))
                OnSceneStop();
        }

        if (!has_active_scene) {
            ImGui::EndDisabled();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
    }

    void EditorLayer::RenderViewport() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });

        std::string viewport_title = "Viewport###Viewport";
        if (Loom::Project::GetActive()) {
            if (!mCurrentScenePath.empty()) {
                std::filesystem::path path = mCurrentScenePath;
                viewport_title = path.filename().string() + " (Viewport)###Viewport";
            } else {
                viewport_title = "Untitled Scene (Viewport)###Viewport";
            }
        }

        ImGui::Begin(viewport_title.c_str());

        mViewportFocused = ImGui::IsWindowFocused();
        mViewportHovered = ImGui::IsWindowHovered();
        Loom::Application::Get().GetImGuiLayer()->BlockEvents(!mViewportHovered);

        UpdateViewportBounds();
        UpdateViewportSize();

        uint32_t texture_id = mFramebuffer->GetColorAttachmentRendererID(0);
        ImGui::Image((void*)(intptr_t)texture_id, ImVec2{ mViewportSize.x, mViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

        RenderGizmos();

        RenderToolbar();

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorLayer::UpdateViewportBounds() {
        auto min_region = ImGui::GetWindowContentRegionMin();
        auto max_region = ImGui::GetWindowContentRegionMax();
        auto offset     = ImGui::GetWindowPos();

        mViewportBounds[0] = { min_region.x + offset.x, min_region.y + offset.y };
        mViewportBounds[1] = { max_region.x + offset.x, max_region.y + offset.y };
    }

    void EditorLayer::UpdateViewportSize() {
        ImVec2 content = ImGui::GetContentRegionAvail();
        mViewportSize  = { content.x, content.y };
    }

    void EditorLayer::RenderGizmos() {
        if (mSceneState != SceneState::Edit)   // ADD THIS
            return;

        Loom::Entity selected_entity = mSceneHierarchyPanel.GetSelectedEntity();
        if (!selected_entity || mGizmoType == -1)
            return;

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(mViewportBounds[0].x, mViewportBounds[0].y, mViewportBounds[1].x - mViewportBounds[0].x, mViewportBounds[1].y - mViewportBounds[0].y);

        const glm::mat4& proj = mEditorCamera.GetProjectionMatrix();
        glm::mat4        view = mEditorCamera.GetViewMatrix();

        auto&     tc        = selected_entity.GetComponent<Loom::TransformComponent>();
        glm::mat4 transform = tc.GetTransform();

        bool  snap           = Loom::Input::IsKeyPressed(Loom::Key::LeftControl);
        float snap_value     = (mGizmoType == ImGuizmo::OPERATION::ROTATE) ? 45.0f : 0.5f;
        float snap_values[3] = { snap_value, snap_value, snap_value };

        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), (ImGuizmo::OPERATION)mGizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr, snap ? snap_values : nullptr);

        if (ImGuizmo::IsUsing()) {
            glm::vec3 translation, rotation, scale;
            Loom::Math::DecomposeTransform(transform, translation, rotation, scale);

            glm::vec3 delta_rotation = rotation - tc.Rotation;
            tc.Translation           = translation;
            tc.Rotation += delta_rotation;
            tc.Scale = scale;
        }
    }

#pragma endregion

#pragma region Project Management

    void EditorLayer::NewProject() {
        mShowProjectWizard = true;
    }

    void EditorLayer::OpenProject() {
        constexpr nfdfilteritem_t filters[] = {
            { "Loom Project", "loomproj" },
            { "All Files", "*" },
        };

        NFD::Guard      nfd_guard;
        NFD::UniquePath out_path;
        nfdresult_t     result = NFD::OpenDialog(out_path, filters, 2);

        if (result == NFD_OKAY) {
            OpenProject(out_path.get());
        } else if (result == NFD_ERROR) {
            LOOM_CORE_ERROR("NFD OpenDialog error: {}", NFD::GetError());
        } // else if NFD_CANCEL: user dismissed, do nothing
    }

    void EditorLayer::OpenProject(const std::string& filepath) {
        std::shared_ptr<Loom::Project> project = std::make_shared<Loom::Project>();
        Loom::ProjectSerializer serializer(project);

        if (serializer.Deserialize(filepath)) {
            Loom::Project::SetActive(project);

            std::string title = "Weaver Editor - " + project->GetConfig().Name;
            Loom::Application::Get().GetWindow().SetTitle(title);

            mContentBrowserPanel.Init();

            std::filesystem::path start_scene_path = Loom::Project::GetAssetFileSystemPath(project->GetConfig().StartScene);
            if (std::filesystem::exists(start_scene_path) && !project->GetConfig().StartScene.empty()) {
                OpenScene(start_scene_path.string());
            } else {
                NewScene(); // If no start scene exists, give them a blank slate
            }
        }
    }

    void EditorLayer::SaveProjectAs() {
        constexpr nfdfilteritem_t filters[] = {
            { "Loom Project", "loomproj" },
            { "All Files", "*" },
        };

        NFD::Guard      nfd_guard;
        NFD::UniquePath out_path;
        nfdresult_t     result = NFD::SaveDialog(out_path, filters, 2, nullptr, "MyProject.loomproj");

        if (result == NFD_OKAY) {
            std::filesystem::path path = out_path.get();
            if (path.extension() != ".loomproj")
                path += ".loomproj";

            std::filesystem::create_directories(path.parent_path());

            Loom::ProjectSerializer serializer(Loom::Project::GetActive());
            serializer.Serialize(path.string());
        } else if (result == NFD_ERROR) {
            LOOM_CORE_ERROR("NFD SaveDialog error: {}", NFD::GetError());
        }
    }

    void EditorLayer::RenderProjectWizard() {
        if (mShowProjectWizard) {
            ImGui::OpenPopup("New Project Wizard");
            mShowProjectWizard = false;
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("New Project Wizard", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Create a New Loom Engine Project");
            ImGui::Separator();

            ImGui::InputText("Project Name", mNewProjectName, sizeof(mNewProjectName));

            ImGui::Text("Location: %s", mNewProjectPath.empty() ? "Not Selected" : mNewProjectPath.c_str());
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) {
                NFD::Guard      nfd_guard;
                NFD::UniquePath out_path;
                nfdresult_t     result = NFD::PickFolder(out_path);
                if (result == NFD_OKAY) {
                    mNewProjectPath = out_path.get();
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            bool can_create = !mNewProjectPath.empty() && strlen(mNewProjectName) > 0;
            if (!can_create) ImGui::BeginDisabled();

            if (ImGui::Button("Create Project", ImVec2(120, 0))) {
                // Directory Generation
                std::filesystem::path root_dir = std::filesystem::path(mNewProjectPath) / mNewProjectName;
                std::filesystem::path asset_dir = root_dir / "assets";

                // 1. Create the physical folders on the hard drive
                std::filesystem::create_directories(asset_dir / "scenes");
                std::filesystem::create_directories(asset_dir / "textures");
                std::filesystem::create_directories(asset_dir / "scripts");

                // 2. Set up the Project object in memory
                std::shared_ptr<Loom::Project> new_project = std::make_shared<Loom::Project>();
                new_project->GetConfig().Name = mNewProjectName;
                new_project->GetConfig().AssetDirectory = "assets";

                // 3. Serialize the .loomproj file
                std::filesystem::path proj_file_path = root_dir / (std::string(mNewProjectName) + ".loomproj");
                Loom::ProjectSerializer serializer(new_project);
                serializer.Serialize(proj_file_path.string());

                // 4. Set it activates and boot the editor
                Loom::Project::SetActive(new_project);
                std::string title = "Weaver Editor - " + std::string(mNewProjectName);
                Loom::Application::Get().GetWindow().SetTitle(title);
                mContentBrowserPanel.Init();
                NewScene();

                LOOM_CORE_INFO("Created new project at: {0}", root_dir.string());
                ImGui::CloseCurrentPopup();
            }

            if (!can_create) ImGui::EndDisabled();

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

#pragma endregion

#pragma region Scene Management

    void EditorLayer::NewScene() {
        mActiveScene = std::make_shared<Loom::Scene>();
        mSceneHierarchyPanel.SetContext(mActiveScene);
        mCurrentScenePath.clear();
    }

    void EditorLayer::OpenScene() {
        constexpr nfdfilteritem_t filters[] = {
            { "Loom Scene", "loom" },
            { "All Files", "*" },
        };

        NFD::Guard      nfd_guard;
        NFD::UniquePath out_path;
        nfdresult_t     result = NFD::OpenDialog(out_path, filters, 2);

        if (result == NFD_OKAY) {
            OpenScene(out_path.get());
        } else if (result == NFD_ERROR) {
            LOOM_CORE_ERROR("NFD OpenDialog error: {}", NFD::GetError());
        } // else if NFD_CANCEL: user dismissed, do nothing
    }

    void EditorLayer::OpenScene(const std::string& filepath) {
        if (!std::filesystem::exists(filepath)) {
            LOOM_CORE_WARN("EditorLayer: scene file '{}' does not exist", filepath);
            return;
        }

        auto new_scene = std::make_shared<Loom::Scene>();

        Loom::SceneSerializer serializer(new_scene);

        if (serializer.Deserialize(filepath)) {
            mEditorScene = new_scene;
            mSceneHierarchyPanel.SetContext(mEditorScene);

            mActiveScene      = mEditorScene;
            mCurrentScenePath = filepath;
        }
    }

    void EditorLayer::SaveScene() {
        if (mCurrentScenePath.empty()) {
            SaveSceneAs();
            return;
        }
        Loom::SceneSerializer serializer(mActiveScene);
        serializer.Serialize(mCurrentScenePath);
    }

    void EditorLayer::SaveSceneAs() {
        constexpr nfdfilteritem_t filters[] = {
            { "Loom Scene", "loom" },
            { "All Files", "*" },
        };

        NFD::Guard      nfd_guard;
        NFD::UniquePath out_path;
        nfdresult_t     result = NFD::SaveDialog(out_path, filters, 2, nullptr, "scene.loom");

        if (result == NFD_OKAY) {
            // nfd-extended does NOT append the extension automatically on all platforms,
            // so we ensure .loom is present.
            std::filesystem::path path = out_path.get();
            if (path.extension() != ".loom")
                path += ".loom";

            std::filesystem::create_directories(path.parent_path());

            Loom::SceneSerializer serializer(mActiveScene);
            serializer.Serialize(path.string());
            mCurrentScenePath = path.string();

            // Scene Management Connection:
            // Update the project's start scene if it doesn't have one, making it
            // relative to the active asset directory.
            auto active_project = Loom::Project::GetActive();
            if (active_project && active_project->GetConfig().StartScene.empty()) {
                std::filesystem::path asset_dir = Loom::Project::GetAssetDirectory();
                std::filesystem::path relative_scene_path = std::filesystem::relative(path, asset_dir);

                active_project->GetConfig().StartScene = relative_scene_path;
                LOOM_CORE_INFO("Set project StartScene to {}", relative_scene_path.string());
            }
        } else if (result == NFD_ERROR) {
            LOOM_CORE_ERROR("NFD SaveDialog error: {}", NFD::GetError());
        }
    }

    void EditorLayer::OnScenePlay() {
        mSceneState  = SceneState::Play;
        mActiveScene = Loom::Scene::Copy(mEditorScene);
        mSceneHierarchyPanel.SetContext(mActiveScene);
    }

    void EditorLayer::OnSceneStop() {
        mActiveScene->OnRuntimeStop();
        mSceneState  = SceneState::Edit;
        mActiveScene = mEditorScene;
        mSceneHierarchyPanel.SetContext(mActiveScene);
    }

#pragma endregion

} // namespace Weaver
