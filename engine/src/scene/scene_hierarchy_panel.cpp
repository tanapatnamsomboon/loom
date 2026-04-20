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
        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", tag.c_str());

        if (ImGui::IsItemClicked()) {
            mSelectionContext = entity;
        }

        if (opened) {
            ImGui::TreePop();
        }
    }

    void SceneHierarchyPanel::DrawComponents(Entity entity) {
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