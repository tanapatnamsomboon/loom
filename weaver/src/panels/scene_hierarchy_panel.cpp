#include "scene_hierarchy_panel.h"
#include "loom/scene/components.h"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

namespace Weaver {
    SceneHierarchyPanel::SceneHierarchyPanel(const std::shared_ptr<Loom::Scene>& context) {
        SetContext(context);
    }

    void SceneHierarchyPanel::SetContext(const std::shared_ptr<Loom::Scene>& context) {
        mContext = context;
        mSelectionContext = {};
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
                auto& src = entity.GetComponent<Loom::SpriteRendererComponent>();

                ImGui::ColorEdit4("Color", glm::value_ptr(src.Color));

                ImGui::TreePop();
            }
        }

        if (entity.HasComponent<Loom::CameraComponent>()) {
            if (ImGui::TreeNodeEx((void*)typeid(Loom::CameraComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Camera")) {
                auto& camera_component = entity.GetComponent<Loom::CameraComponent>();
                auto& camera = camera_component.Camera;

                ImGui::Checkbox("Primary", &camera_component.Primary);

                float ortho_size = camera.GetOrthographicSize();
                if (ImGui::DragFloat("Size", &ortho_size)) {
                    camera.SetOrthographicSize(ortho_size);
                }

                ImGui::TreePop();
            }
        }
    }

} // namespace Loom