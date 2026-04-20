#include "loom/scene/scene_hierarchy_panel.h"
#include "loom/scene/components.h"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

namespace Loom {
    SceneHierarchyPanel::SceneHierarchyPanel(const std::shared_ptr<Scene>& context) {
        SetContext(context);
    }

    void SceneHierarchyPanel::SetContext(const std::shared_ptr<Scene>& context) {
        mContext = context;
        mSelectionContext = {};
    }

    void SceneHierarchyPanel::OnImGuiRender() {
        ImGui::Begin("Scene Hierarchy");

        auto view = mContext->mRegistry.view<entt::entity>();
        for (auto entity_id : view) {
            Entity entity{ entity_id, mContext.get() };
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

    void SceneHierarchyPanel::DrawEntityNode(Entity entity) {
        auto& tag = entity.GetComponent<TagComponent>().Tag;

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

    void SceneHierarchyPanel::DrawComponents(Entity entity) {
        if (entity.HasComponent<IDComponent>()) {
            auto& uuid = entity.GetComponent<IDComponent>().ID;

            ImGui::Text("UUID: %llu", (uint64_t)uuid);
            ImGui::Separator();
        }

        if (entity.HasComponent<TagComponent>()) {
            auto& tag = entity.GetComponent<TagComponent>().Tag;

            char buffer[256] = {};
            strncpy(buffer, tag.c_str(), sizeof(buffer));

            if (ImGui::InputText("Tag", buffer, sizeof(buffer))) {
                tag = std::string(buffer);
            }
        }

        if (entity.HasComponent<TransformComponent>()) {
            if (ImGui::TreeNodeEx((void*)typeid(TransformComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Transform")) {
                auto& transform = entity.GetComponent<TransformComponent>();

                ImGui::DragFloat3("Position", glm::value_ptr(transform.Translation), 0.1f);

                glm::vec3 rotation = glm::degrees(transform.Rotation);
                if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 1.0f))
                    transform.Rotation = glm::radians(rotation);

                ImGui::DragFloat3("Scale", glm::value_ptr(transform.Scale), 0.1f);

                ImGui::TreePop();
            }
        }

        if (entity.HasComponent<SpriteRendererComponent>()) {
            if (ImGui::TreeNodeEx((void*)typeid(SpriteRendererComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Sprite Renderer")) {
                auto& src = entity.GetComponent<SpriteRendererComponent>();

                ImGui::ColorEdit4("Color", glm::value_ptr(src.Color));

                ImGui::TreePop();
            }
        }
    }

} // namespace Loom