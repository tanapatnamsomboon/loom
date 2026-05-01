#include "scene_hierarchy_panel.h"
#include <loom/asset/asset_manager.h>
#include <loom/scene/components.h>
#include <loom/scene/script_registry.h>
#include <nfd.hpp>
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
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
                if (mSceneModifiedCallback) mSceneModifiedCallback();
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

            if (mSceneModifiedCallback) mSceneModifiedCallback();
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
                if (mSceneModifiedCallback) mSceneModifiedCallback();
            }
        }

        ImGui::SameLine();
        ImGui::PushItemWidth(-1);
        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("AddComponent");
        }
        ImGui::PopItemWidth();

        if (ImGui::BeginPopup("AddComponent")) {
            if (!mSelectionContext.HasComponent<Loom::CameraComponent>()) {
                if (ImGui::MenuItem("Camera")) {
                    mSelectionContext.AddComponent<Loom::CameraComponent>();
                    if (mSceneModifiedCallback) mSceneModifiedCallback();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!mSelectionContext.HasComponent<Loom::SpriteRendererComponent>()) {
                if (ImGui::MenuItem("Sprite Renderer")) {
                    mSelectionContext.AddComponent<Loom::SpriteRendererComponent>();
                    if (mSceneModifiedCallback) mSceneModifiedCallback();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!mSelectionContext.HasComponent<Loom::NativeScriptComponent>()) {
                if (ImGui::MenuItem("Script")) {
                    mSelectionContext.AddComponent<Loom::NativeScriptComponent>();
                    if (mSceneModifiedCallback) mSceneModifiedCallback();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }

        // Transform Component
        if (entity.HasComponent<Loom::TransformComponent>()) {
            if (ImGui::TreeNodeEx((void*)typeid(Loom::TransformComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Transform")) {
                auto& transform = entity.GetComponent<Loom::TransformComponent>();
                bool is_modified = false;

                is_modified |= ImGui::DragFloat3("Position", glm::value_ptr(transform.Translation), 0.1f);

                glm::vec3 rotation = glm::degrees(transform.Rotation);
                if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 0.1f)) {
                    transform.Rotation = glm::radians(rotation);
                    is_modified = true;
                }

                is_modified |= ImGui::DragFloat3("Scale", glm::value_ptr(transform.Scale), 0.1f);

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();

                ImGui::TreePop();
            }
        }

        // Sprite Renderer Component
        if (entity.HasComponent<Loom::SpriteRendererComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::SpriteRendererComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Sprite Renderer");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& src         = entity.GetComponent<Loom::SpriteRendererComponent>();
                auto& texture     = src.Texture;
                bool  is_modified = false;

                ImTextureID texture_to_display = (ImTextureID)(uintptr_t)((texture != nullptr) ? texture->GetRendererID() : mCheckerboard->GetRendererID());
                std::string label_text         = (texture != nullptr) ? std::filesystem::path(texture->GetPath()).filename().string() : "None (Select...)";

                ImGui::PushID("TextureSlot1");
                ImGui::Image(texture_to_display, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 0.5f));
                ImGui::SameLine();
                if (ImGui::Button(label_text.c_str(), ImVec2(150, 0))) {
                    auto new_texture = LoadTexture();
                    if (new_texture) {
                        texture = new_texture;
                        is_modified = true;
                    }
                }

                if (texture) {
                    ImGui::SameLine();
                    if (ImGui::Button("X")) {
                        texture = nullptr;
                        is_modified = true;
                    }
                }
                ImGui::PopID();

                is_modified |= ImGui::ColorEdit4("Color", glm::value_ptr(src.Color));
                is_modified |= ImGui::DragFloat("Tiling Factor", &src.TilingFactor, 0.1f, 0.1f, 100.0f);

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();

                ImGui::TreePop();
            }

            if (remove_component) {
                entity.RemoveComponent<Loom::SpriteRendererComponent>();
                if (mSceneModifiedCallback) mSceneModifiedCallback();
            }
        }

        // Camera Component
        if (entity.HasComponent<Loom::CameraComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::CameraComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Camera");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& cc          = entity.GetComponent<Loom::CameraComponent>();
                auto& camera      = cc.Camera;
                bool  is_modified = false;

                is_modified |= ImGui::Checkbox("Primary", &cc.Primary);

                const char* projection_type_strings[] = { "Perspective", "Orthographic" };
                const char* current_projection_string = projection_type_strings[(int)camera.GetProjectionType()];

                if (ImGui::BeginCombo("Projection", current_projection_string)) {
                    for (int i = 0; i < 2; i++) {
                        bool is_selected = current_projection_string == projection_type_strings[i];
                        if (ImGui::Selectable(projection_type_strings[i], is_selected)) {
                            current_projection_string = projection_type_strings[i];
                            camera.SetProjectionType((Loom::SceneCamera::ProjectionType)i);
                            is_modified = true;
                        }
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (camera.GetProjectionType() == Loom::SceneCamera::ProjectionType::Perspective) {
                    float vertical_fov = glm::degrees(camera.GetPerspectiveVerticalFOV());
                    float near_clip    = camera.GetPerspectiveNearClip();
                    float far_clip     = camera.GetPerspectiveFarClip();

                    if (ImGui::DragFloat("Vertical FOV", &vertical_fov) || ImGui::DragFloat("Near Clip", &near_clip) || ImGui::DragFloat("Far Clip", &far_clip)) {
                        camera.SetPerspective(glm::radians(vertical_fov), near_clip, far_clip);
                        is_modified = true;
                    }
                } else {
                    float ortho_size = camera.GetOrthographicSize();
                    float near_clip  = camera.GetOrthographicNearClip();
                    float far_clip   = camera.GetOrthographicFarClip();

                    if (ImGui::DragFloat("Size", &ortho_size) || ImGui::DragFloat("Near Clip", &near_clip) || ImGui::DragFloat("Far Clip", &far_clip)) {
                        camera.SetOrthographic(ortho_size, near_clip, far_clip);
                        is_modified = true;
                    }
                }

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();

                ImGui::TreePop();
            }

            if (remove_component) {
                entity.RemoveComponent<Loom::CameraComponent>();
                if (mSceneModifiedCallback) mSceneModifiedCallback();
            }
        }

        // Native Script Component
        if (entity.HasComponent<Loom::NativeScriptComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::NativeScriptComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Script");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& nsc         = entity.GetComponent<Loom::NativeScriptComponent>();
                bool  is_modified = false;

                const auto& names   = Loom::ScriptRegistry::GetNames();
                const char* current = nsc.ScriptName.empty() ? "None" : nsc.ScriptName.c_str();

                if (ImGui::BeginCombo("Script", current)) {
                    if (ImGui::Selectable("None", nsc.ScriptName.empty())) {
                        nsc = Loom::NativeScriptComponent{};
                        is_modified = true;
                    }
                    for (const auto& name : names) {
                        bool selected = (nsc.ScriptName == name);
                        if (ImGui::Selectable(name.c_str(), selected)) {
                            if (nsc.Instance && nsc.DestroyScript)
                                nsc.DestroyScript(&nsc);
                            nsc.BindByName(name);
                            is_modified = true;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (nsc.IsValid() && nsc.Instance) {
                    ImGui::TextDisabled("(running)");
                }

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();

                ImGui::TreePop();
            }

            if (remove_component) {
                entity.RemoveComponent<Loom::NativeScriptComponent>();
                if (mSceneModifiedCallback) mSceneModifiedCallback();
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
            return Loom::AssetManager::GetTexture(out_path.get());
        } else if (result == NFD_ERROR) {
            LOOM_CORE_ERROR("NFD OpenDialog error: {}", NFD::GetError());
        }

        return nullptr;
    }

} // namespace Weaver
