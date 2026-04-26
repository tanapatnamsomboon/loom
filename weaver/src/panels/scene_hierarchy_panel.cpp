#include "scene_hierarchy_panel.h"
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <loom/scene/components.h>
#include <nfd.hpp>
#include <filesystem>

namespace Weaver {
    SceneHierarchyPanel::SceneHierarchyPanel(const std::shared_ptr<Loom::Scene>& context) {
        SetContext(context);
    }

    void SceneHierarchyPanel::SetContext(const std::shared_ptr<Loom::Scene>& context) {
        mContext          = context;
        mSelectionContext = {};
    }

    void SceneHierarchyPanel::Init() {
        mCheckerboard           = Loom::Texture2D::Create(2, 2);
        uint32_t checkerboard[] = { 0xFFFFFFFF, 0xFFCCCCCC, 0xFFCCCCCC, 0xFFFFFFFF };
        mCheckerboard->SetData(checkerboard, sizeof(uint32_t) * 4);
    }

    void SceneHierarchyPanel::OnImGuiRender() {
        ImGui::Begin("Scene Hierarchy");

        auto view = mContext->GetAllEntitiesWith<Loom::TagComponent>();
        for (auto entity_id : view) {
            Loom::Entity entity{ entity_id, mContext.get() };
            DrawEntityNode(entity);
        }

        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
            mSelectionContext = {};

        if (ImGui::BeginPopupContextWindow("HierarchyContextWindow", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Empty Entity")) {
                mContext->CreateEntity("Empty Entity");
            }
            ImGui::EndPopup();
        }

        ImGui::End();

        ImGui::Begin("Properties");
        if (mSelectionContext) {
            DrawComponents(mSelectionContext);
        }
        ImGui::End();
    }

    void SceneHierarchyPanel::DrawEntityNode(Loom::Entity entity) {
        auto& tag = entity.GetComponent<Loom::TagComponent>().Tag;

        ImGuiTreeNodeFlags flags = ((mSelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
        flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", tag.c_str());

        if (ImGui::IsItemClicked()) {
            mSelectionContext = entity;
        }

        bool entity_deleted = false;
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Entity")) {
                entity_deleted = true;
            }
            ImGui::EndPopup();
        }

        if (opened) {
            ImGui::TreePop();
        }

        if (entity_deleted) {
            mContext->DestroyEntity(entity);

            if (mSelectionContext == entity) {
                mSelectionContext = {};
            }
        }
    }

    void SceneHierarchyPanel::DrawComponents(Loom::Entity entity) {
        if (entity.HasComponent<Loom::IDComponent>()) {
            auto& uuid = entity.GetComponent<Loom::IDComponent>().ID;

            ImGui::Text("UUID: %llu", (uint64_t)uuid);
            ImGui::Separator();
        }

        if (entity.HasComponent<Loom::TagComponent>()) {
            auto& tag = entity.GetComponent<Loom::TagComponent>().Tag;

            char buffer[256] = {};
            strncpy(buffer, tag.c_str(), sizeof(buffer));

            if (ImGui::InputText("##Tag", buffer, sizeof(buffer))) {
                tag = std::string(buffer);
            }
        }

        ImGui::SameLine();
        ImGui::PushItemWidth(-1);
        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("AddComponent");
        }
        ImGui::PopItemWidth();

        if (ImGui::BeginPopup("AddComponent")) {
            if (ImGui::MenuItem("Camera")) {
                mSelectionContext.AddComponent<Loom::CameraComponent>();
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Sprite Renderer")) {
                mSelectionContext.AddComponent<Loom::SpriteRendererComponent>();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (entity.HasComponent<Loom::TransformComponent>()) {
            if (ImGui::TreeNodeEx((void*)typeid(Loom::TransformComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Transform")) {
                auto& transform = entity.GetComponent<Loom::TransformComponent>();

                ImGui::DragFloat3("Position", glm::value_ptr(transform.Translation), 0.1f);

                glm::vec3 rotation = glm::degrees(transform.Rotation);
                if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 0.1f)) {
                    transform.Rotation = glm::radians(rotation);
                }

                ImGui::DragFloat3("Scale", glm::value_ptr(transform.Scale), 0.1f);

                ImGui::TreePop();
            }
        }

        if (entity.HasComponent<Loom::SpriteRendererComponent>()) {
            if (ImGui::TreeNodeEx((void*)typeid(Loom::SpriteRendererComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Sprite Renderer")) {
                auto& src     = entity.GetComponent<Loom::SpriteRendererComponent>();
                auto& texture = src.Texture;
                auto& color   = src.Color;

                ImTextureID texture_to_display = (ImTextureID)(uintptr_t)((texture != nullptr) ? texture->GetRendererID() : mCheckerboard->GetRendererID());
                std::string label_text         = (texture != nullptr) ? std::filesystem::path(texture->GetPath()).filename().string() : "None (Select...)";

                ImGui::PushID("TextureSlot1");
                ImGui::Image(texture_to_display, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 0.5f));
                ImGui::SameLine();
                if (ImGui::Button(label_text.c_str(), ImVec2(150, 0))) {
                    texture = LoadTexture();
                }

                if (texture) {
                    ImGui::SameLine();
                    if (ImGui::Button("X")) {
                        texture = nullptr;
                    }
                }
                ImGui::PopID();

                ImGui::ColorEdit4("Color", glm::value_ptr(src.Color));
                ImGui::DragFloat("Tiling Factor", &src.TilingFactor, 0.1f, 0.1f, 100.0f);
                ImGui::TreePop();
            }
        }

        if (entity.HasComponent<Loom::CameraComponent>()) {
            if (ImGui::TreeNodeEx((void*)typeid(Loom::CameraComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Camera")) {
                auto& camera_component = entity.GetComponent<Loom::CameraComponent>();
                auto& camera           = camera_component.Camera;

                ImGui::Checkbox("Primary", &camera_component.Primary);

                const char* projection_type_strings[] = { "Perspective", "Orthographic" };
                const char* current_projection_string = projection_type_strings[(int)camera.GetProjectionType()];

                if (ImGui::BeginCombo("Projection", current_projection_string)) {
                    for (int i = 0; i < 2; i++) {
                        bool is_selected = current_projection_string == projection_type_strings[i];
                        if (ImGui::Selectable(projection_type_strings[i], is_selected)) {
                            current_projection_string = projection_type_strings[i];
                            camera.SetProjectionType((Loom::SceneCamera::ProjectionType)i);
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (camera.GetProjectionType() == Loom::SceneCamera::ProjectionType::Perspective) {
                    float vertical_fov = glm::degrees(camera.GetPerspectiveVerticalFOV());
                    if (ImGui::DragFloat("Vertical FOV", &vertical_fov)) {
                        camera.SetPerspectiveVerticalFOV(glm::radians(vertical_fov));
                    }
                } else {
                    float ortho_size = camera.GetOrthographicSize();
                    if (ImGui::DragFloat("Size", &ortho_size)) {
                        camera.SetOrthographicSize(ortho_size);
                    }
                }

                ImGui::TreePop();
            }
        }
    }

    std::shared_ptr<Loom::Texture2D> SceneHierarchyPanel::LoadTexture() {
        constexpr nfdfilteritem_t filters[] = {
            { "Images", "png,jpg,jpeg,bmp,tga" },
            { "All Files", "*" },
        };

        NFD::Guard      nfd_guard;
        NFD::UniquePath out_path;
        nfdresult_t     result = NFD::OpenDialog(out_path, filters, 2);

        if (result == NFD_OKAY) {
            return Loom::Texture2D::Create(out_path.get());
        } else if (result == NFD_ERROR) {
            LOOM_CORE_ERROR("NFD OpenDialog error: {}", NFD::GetError());
        }

        return nullptr;
    }

} // namespace Weaver
