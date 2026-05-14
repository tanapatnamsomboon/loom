#include "scene_hierarchy_panel.h"
#include "editor/commands.h"
#include "editor/file_dialog.h"
#include <loom/asset/asset_manager.h>
#include <loom/scene/components.h>
#include <loom/scene/scene_serializer.h>
#include <loom/scene/script_registry.h>
#include <imgui.h>
#include <loom/core/application.h>
#include <glm/gtc/type_ptr.hpp>
#include <loom/project/project.h>
#include <loom/scripting/scripting_engine.h>
#include <algorithm>
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

        // Only draw root entities; children are drawn recursively
        auto view = mContext->GetAllEntitiesWith<Loom::TagComponent>();
        for (auto entity_id : view) {
            Loom::Entity entity{ entity_id, mContext.get() };
            if (entity.HasComponent<Loom::RelationshipComponent>() &&
                entity.GetComponent<Loom::RelationshipComponent>().Parent != entt::null)
                continue;
            DrawEntityNode(entity);
        }

        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
            mSelectionContext = {};

        // Invisible fill — drop here to detach from parent (move to root)
        ImVec2 remaining = ImGui::GetContentRegionAvail();
        if (remaining.y > 0.0f) {
            ImGui::Dummy(remaining);
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
                    uint32_t     dragged_id     = *(const uint32_t*)payload->Data;
                    Loom::Entity dragged_entity { (entt::entity)dragged_id, mContext.get() };
                    if (dragged_entity) {
                        mContext->RemoveParent(dragged_entity);
                        if (mSceneModifiedCallback) mSceneModifiedCallback();
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        if (ImGui::BeginPopupContextWindow("HierarchyContextWindow", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Empty Entity")) {
                if (mCommandCallback) {
                    auto cmd = std::make_unique<EntityCreateCommand>(mContext, "Empty Entity");
                    auto* raw = cmd.get();  // valid after move since history holds the object
                    mCommandCallback(std::move(cmd));
                    if (raw->GetCreatedUUID())
                        mSelectionContext = mContext->GetEntityByUUID(Loom::UUID(raw->GetCreatedUUID()));
                } else {
                    mSelectionContext = mContext->CreateEntity("Empty Entity");
                    if (mSceneModifiedCallback) mSceneModifiedCallback();
                }
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

        bool has_children = entity.HasComponent<Loom::RelationshipComponent>() &&
                            !entity.GetComponent<Loom::RelationshipComponent>().Children.empty();
        bool has_parent   = entity.HasComponent<Loom::RelationshipComponent>() &&
                            entity.GetComponent<Loom::RelationshipComponent>().Parent != entt::null;

        ImGuiTreeNodeFlags flags = ((mSelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) |
                                    ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!has_children)
            flags |= ImGuiTreeNodeFlags_Leaf;

        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", tag.c_str());

        if (ImGui::IsItemClicked())
            mSelectionContext = entity;

        // Drag source — payload is the raw uint32 entity handle
        if (ImGui::BeginDragDropSource()) {
            uint32_t entity_id = (uint32_t)entity;
            ImGui::SetDragDropPayload("ENTITY_ID", &entity_id, sizeof(uint32_t));
            ImGui::TextUnformatted(tag.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop target — reparent dragged entity onto this one
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
                uint32_t     dragged_id     = *(const uint32_t*)payload->Data;
                Loom::Entity dragged_entity { (entt::entity)dragged_id, mContext.get() };
                if (dragged_entity && dragged_entity != entity) {
                    mContext->SetParent(dragged_entity, entity);
                    if (mSceneModifiedCallback) mSceneModifiedCallback();
                }
            }
            ImGui::EndDragDropTarget();
        }

        bool entity_deleted = false;
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Create Child Entity")) {
                uint64_t parent_uuid = (uint64_t)entity.GetComponent<Loom::IDComponent>().ID;
                if (mCommandCallback) {
                    auto cmd = std::make_unique<EntityCreateCommand>(mContext, "Child Entity", parent_uuid);
                    auto* raw = cmd.get();
                    mCommandCallback(std::move(cmd));
                    if (raw->GetCreatedUUID())
                        mSelectionContext = mContext->GetEntityByUUID(Loom::UUID(raw->GetCreatedUUID()));
                } else {
                    Loom::Entity child = mContext->CreateEntity("Child Entity");
                    mContext->SetParent(child, entity);
                    if (mSceneModifiedCallback) mSceneModifiedCallback();
                }
            }
            if (has_parent) {
                if (ImGui::MenuItem("Detach from Parent")) {
                    mContext->RemoveParent(entity);
                    if (mSceneModifiedCallback) mSceneModifiedCallback();
                }
            }
            if (ImGui::MenuItem("Save as Prefab...")) {
                auto uuid     = entity.GetComponent<Loom::IDComponent>().ID;
                auto scene    = mContext;
                auto tag      = entity.GetComponent<Loom::TagComponent>().Tag;
                FileDialog::Save("SaveAsPrefab", "Save as Prefab", ".lprefab", tag + ".lprefab",
                    [uuid, scene](const std::string& picked) {
                        std::filesystem::path path = picked;
                        if (path.extension() != ".lprefab") path += ".lprefab";
                        Loom::Entity e = scene->GetEntityByUUID(uuid);
                        if (!e) return;
                        Loom::SceneSerializer(scene).SerializePrefab(path.string(), e);
                    });
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Entity"))
                entity_deleted = true;
            ImGui::EndPopup();
        }

        if (opened) {
            if (has_children) {
                for (auto child : entity.GetChildren())
                    DrawEntityNode(child);
            }
            ImGui::TreePop();
        }

        if (entity_deleted) {
            bool clear_selection = (mSelectionContext == entity);
            if (!clear_selection && mSelectionContext) {
                Loom::Entity check = mSelectionContext;
                while (check.HasComponent<Loom::RelationshipComponent>()) {
                    Loom::Entity parent = check.GetParent();
                    if (!parent) break;
                    if (parent == entity) { clear_selection = true; break; }
                    check = parent;
                }
            }
            if (clear_selection) mSelectionContext = {};
            if (mCommandCallback) {
                mCommandCallback(std::make_unique<EntityDeleteCommand>(mContext, entity));
            } else {
                mContext->DestroyEntity(entity);
                if (mSceneModifiedCallback) mSceneModifiedCallback();
            }
        }
    }

    void SceneHierarchyPanel::DrawComponents(Loom::Entity entity) {
        Loom::UUID entity_uuid;
        if (entity.HasComponent<Loom::IDComponent>()) {
            entity_uuid = entity.GetComponent<Loom::IDComponent>().ID;
            ImGui::Text("UUID: %llu", (uint64_t)entity_uuid);
            ImGui::Separator();
        }

        // Helper: push a RemoveComponentCommand if a callback is set, else remove directly.
        auto push_remove = [&]<typename T>(const char* label) {
            if (mCommandCallback)
                mCommandCallback(std::make_unique<RemoveComponentCommand<T>>(mContext, entity, label));
            else {
                entity.template RemoveComponent<T>();
                if (mSceneModifiedCallback) mSceneModifiedCallback();
            }
        };

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
            auto push_add = [&]<typename T>(const char* label) {
                if (!mSelectionContext.template HasComponent<T>()) {
                    if (ImGui::MenuItem(label)) {
                        Loom::UUID uuid = mSelectionContext.template GetComponent<Loom::IDComponent>().ID;
                        if (mCommandCallback)
                            mCommandCallback(std::make_unique<AddComponentCommand<T>>(mContext, uuid, label));
                        else {
                            mSelectionContext.template AddComponent<T>();
                            if (mSceneModifiedCallback) mSceneModifiedCallback();
                        }
                        ImGui::CloseCurrentPopup();
                    }
                }
            };
            push_add.template operator()<Loom::CameraComponent>("Camera");
            push_add.template operator()<Loom::SpriteRendererComponent>("Sprite Renderer");
            push_add.template operator()<Loom::MeshRendererComponent>("Mesh Renderer");
            push_add.template operator()<Loom::DirectionalLightComponent>("Directional Light");
            push_add.template operator()<Loom::PointLightComponent>("Point Light");
            push_add.template operator()<Loom::NativeScriptComponent>("Script");
            push_add.template operator()<Loom::Rigidbody2DComponent>("Rigidbody 2D");
            push_add.template operator()<Loom::BoxCollider2DComponent>("Box Collider 2D");
            push_add.template operator()<Loom::CircleCollider2DComponent>("Circle Collider 2D");
            push_add.template operator()<Loom::LuaScriptComponent>("Lua Script");
            push_add.template operator()<Loom::AnimationComponent>("Sprite Animator");
            push_add.template operator()<Loom::AudioSourceComponent>("Audio Source");
            push_add.template operator()<Loom::TextComponent>("Text");
            push_add.template operator()<Loom::TilemapComponent>("Tilemap");
            push_add.template operator()<Loom::ParticleComponent>("Particles");
            ImGui::EndPopup();
        }

        if (entity.HasComponent<Loom::TransformComponent>()) {
            if (ImGui::TreeNodeEx((void*)typeid(Loom::TransformComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Transform")) {
                auto& tc = entity.GetComponent<Loom::TransformComponent>();
                bool is_modified = false;

                // Drag-start snapshots for PropertyEditCommand (captured on activation frame,
                // before the widget has applied any changes).
                static glm::vec3 s_pos_before, s_rot_before, s_scale_before;

                // Position
                is_modified |= ImGui::DragFloat3("Position", glm::value_ptr(tc.Translation), 0.1f);
                if (ImGui::IsItemActivated()) s_pos_before = tc.Translation;
                if (ImGui::IsItemDeactivatedAfterEdit() && mCommandCallback)
                    mCommandCallback(std::make_unique<PropertyEditCommand<glm::vec3>>(
                        mContext, entity_uuid, s_pos_before, tc.Translation,
                        [](Loom::Entity e, const glm::vec3& v) { e.GetComponent<Loom::TransformComponent>().Translation = v; },
                        "Move"));

                // Rotation (inspector shows degrees; engine stores radians)
                glm::vec3 rotation_deg = glm::degrees(tc.Rotation);
                if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotation_deg), 0.1f)) {
                    tc.Rotation = glm::radians(rotation_deg);
                    is_modified = true;
                }
                if (ImGui::IsItemActivated()) s_rot_before = tc.Rotation;
                if (ImGui::IsItemDeactivatedAfterEdit() && mCommandCallback)
                    mCommandCallback(std::make_unique<PropertyEditCommand<glm::vec3>>(
                        mContext, entity_uuid, s_rot_before, tc.Rotation,
                        [](Loom::Entity e, const glm::vec3& v) { e.GetComponent<Loom::TransformComponent>().Rotation = v; },
                        "Rotate"));

                // Scale
                is_modified |= ImGui::DragFloat3("Scale", glm::value_ptr(tc.Scale), 0.1f);
                if (ImGui::IsItemActivated()) s_scale_before = tc.Scale;
                if (ImGui::IsItemDeactivatedAfterEdit() && mCommandCallback)
                    mCommandCallback(std::make_unique<PropertyEditCommand<glm::vec3>>(
                        mContext, entity_uuid, s_scale_before, tc.Scale,
                        [](Loom::Entity e, const glm::vec3& v) { e.GetComponent<Loom::TransformComponent>().Scale = v; },
                        "Scale"));

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }
        }

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
                ImGui::Image(texture_to_display, ImVec2(32, 32), ImVec2(0, 1), ImVec2(1, 0), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 0.5f));
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::filesystem::path dropped((const char*)payload->Data);
                        auto ext = dropped.extension();
                        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
                            auto full = Loom::Project::GetAssetFileSystemPath(dropped);
                            auto new_texture = Loom::AssetManager::GetTexture(full.generic_string(), src.TexSpec);
                            if (new_texture) { texture = new_texture; is_modified = true; }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine();
                if (ImGui::Button(label_text.c_str(), ImVec2(150, 0))) {
                    auto uuid     = entity.GetComponent<Loom::IDComponent>().ID;
                    auto scene    = mContext;
                    auto modified = mSceneModifiedCallback;
                    auto spec     = src.TexSpec;
                    FileDialog::Open("BrowseSpriteTex", "Choose Texture", ".png,.jpg,.jpeg,.bmp,.tga",
                        [uuid, scene, modified, spec](const std::string& abs_path) {
                            Loom::Entity e = scene->GetEntityByUUID(uuid);
                            if (!e || !e.HasComponent<Loom::SpriteRendererComponent>()) return;
                            auto new_texture = Loom::AssetManager::GetTexture(abs_path, spec);
                            if (new_texture) {
                                e.GetComponent<Loom::SpriteRendererComponent>().Texture = new_texture;
                                if (modified) modified();
                            }
                        });
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

                static const char* k_filter_labels[] = { "Nearest", "Linear" };
                int filter_idx = (int)src.TexSpec.Filter;
                if (ImGui::Combo("Filter Mode", &filter_idx, k_filter_labels, 2)) {
                    src.TexSpec.Filter = (Loom::FilterMode)filter_idx;
                    if (src.Texture)
                        src.Texture = Loom::AssetManager::GetTexture(src.Texture->GetPath(), src.TexSpec);
                    is_modified = true;
                }

                static const char* k_wrap_labels[] = { "Repeat", "Clamp" };
                int wrap_idx = (int)src.TexSpec.Wrap;
                if (ImGui::Combo("Wrap Mode", &wrap_idx, k_wrap_labels, 2)) {
                    src.TexSpec.Wrap = (Loom::WrapMode)wrap_idx;
                    if (src.Texture)
                        src.Texture = Loom::AssetManager::GetTexture(src.Texture->GetPath(), src.TexSpec);
                    is_modified = true;
                }

                bool gen_mips = src.TexSpec.GenerateMips;
                if (ImGui::Checkbox("Generate Mipmaps", &gen_mips)) {
                    src.TexSpec.GenerateMips = gen_mips;
                    if (src.Texture)
                        src.Texture = Loom::AssetManager::GetTexture(src.Texture->GetPath(), src.TexSpec);
                    is_modified = true;
                }

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();

                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::SpriteRendererComponent>("Sprite Renderer");
        }

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

            if (remove_component) push_remove.template operator()<Loom::CameraComponent>("Camera");
        }

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

            if (remove_component) push_remove.template operator()<Loom::NativeScriptComponent>("Script");
        }

        if (entity.HasComponent<Loom::Rigidbody2DComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::Rigidbody2DComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Rigidbody 2D");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& rb2d = entity.GetComponent<Loom::Rigidbody2DComponent>();
                bool  is_modified = false;

                const char* body_type_strings[] = { "Static", "Dynamic", "Kinematic" };
                const char* current_body_type_string = body_type_strings[(int)rb2d.Type];

                if (ImGui::BeginCombo("Body Type", current_body_type_string)) {
                    for (int i = 0; i < 3; i++) {
                        bool is_selected = current_body_type_string == body_type_strings[i];
                        if (ImGui::Selectable(body_type_strings[i], is_selected)) {
                            current_body_type_string = body_type_strings[i];
                            rb2d.Type = (Loom::Rigidbody2DComponent::BodyType)i;
                            is_modified = true;
                        }
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                is_modified |= ImGui::Checkbox("Fixed Rotation", &rb2d.FixedRotation);

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::Rigidbody2DComponent>("Rigidbody 2D");
        }

        if (entity.HasComponent<Loom::BoxCollider2DComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::BoxCollider2DComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Box Collider 2D");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& bc2d = entity.GetComponent<Loom::BoxCollider2DComponent>();
                bool  is_modified = false;

                is_modified |= ImGui::DragFloat2("Offset", glm::value_ptr(bc2d.Offset), 0.05f);
                is_modified |= ImGui::DragFloat2("Size", glm::value_ptr(bc2d.Size), 0.05f);
                is_modified |= ImGui::DragFloat("Density", &bc2d.Density, 0.01f, 0.0f, 1.0f);
                is_modified |= ImGui::DragFloat("Friction", &bc2d.Friction, 0.01f, 0.0f, 1.0f);
                is_modified |= ImGui::DragFloat("Restitution", &bc2d.Restitution, 0.01f, 0.0f, 1.0f);
                is_modified |= ImGui::DragFloat("Restitution Threshold", &bc2d.RestitutionThreshold, 0.01f, 0.0f);
                is_modified |= ImGui::Checkbox("Is Sensor", &bc2d.IsSensor);

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::BoxCollider2DComponent>("Box Collider 2D");
        }

        if (entity.HasComponent<Loom::CircleCollider2DComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::CircleCollider2DComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Circle Collider 2D");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& cc2d = entity.GetComponent<Loom::CircleCollider2DComponent>();
                bool  is_modified = false;

                is_modified |= ImGui::DragFloat2("Offset", glm::value_ptr(cc2d.Offset), 0.05f);
                is_modified |= ImGui::DragFloat("Radius", &cc2d.Radius, 0.05f, 0.001f, FLT_MAX);
                is_modified |= ImGui::DragFloat("Density", &cc2d.Density, 0.01f, 0.0f, 1.0f);
                is_modified |= ImGui::DragFloat("Friction", &cc2d.Friction, 0.01f, 0.0f, 1.0f);
                is_modified |= ImGui::DragFloat("Restitution", &cc2d.Restitution, 0.01f, 0.0f, 1.0f);
                is_modified |= ImGui::DragFloat("Restitution Threshold", &cc2d.RestitutionThreshold, 0.01f, 0.0f);
                is_modified |= ImGui::Checkbox("Is Sensor", &cc2d.IsSensor);

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::CircleCollider2DComponent>("Circle Collider 2D");
        }

        if (entity.HasComponent<Loom::LuaScriptComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::LuaScriptComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Lua Script");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& ls = entity.GetComponent<Loom::LuaScriptComponent>();
                bool  is_modified = false;

                char buffer[256] = {};
                strncpy(buffer, ls.ScriptPath.c_str(), sizeof(buffer) - 1);

                // Script row: label | input (fill) | browse button
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Script");
                ImGui::SameLine();
                constexpr float browse_w = 28.0f;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browse_w - ImGui::GetStyle().ItemSpacing.x);
                if (ImGui::InputText("##LuaScriptPath", buffer, sizeof(buffer))) {
                    ls.ScriptPath = buffer;
                    is_modified   = true;
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::filesystem::path dropped((const char*)payload->Data);
                        if (dropped.extension() == ".lua") {
                            ls.ScriptPath = dropped.generic_string();
                            is_modified   = true;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine();
                if (ImGui::Button("...##LuaScriptBrowse", { browse_w, 0.0f })) {
                    auto uuid     = entity.GetComponent<Loom::IDComponent>().ID;
                    auto scene    = mContext;
                    auto modified = mSceneModifiedCallback;
                    FileDialog::Open("BrowseLuaScript", "Choose Lua Script", ".lua",
                        [uuid, scene, modified](const std::string& abs_path) {
                            Loom::Entity e = scene->GetEntityByUUID(uuid);
                            if (!e || !e.HasComponent<Loom::LuaScriptComponent>()) return;
                            e.GetComponent<Loom::LuaScriptComponent>().ScriptPath = FileDialog::MakeAssetRelative(abs_path);
                            if (modified) modified();
                        });
                }

                // Status + actions row
                if (ls.ScriptPath.empty()) {
                    ImGui::TextDisabled("  Drop a .lua file or use '...' to browse");
                } else {
                    // font-independent status dot (IBM Plex Sans Thai doesn't cover Geometric Shapes)
                    {
                        float  lh  = ImGui::GetTextLineHeightWithSpacing();
                        ImVec2 p   = ImGui::GetCursorScreenPos();
                        ImGui::GetWindowDrawList()->AddCircleFilled(
                            { p.x + 5.0f, p.y + lh * 0.5f }, 4.5f, IM_COL32(100, 230, 100, 255));
                        ImGui::Dummy({ 12.0f, lh });
                    }
                    ImGui::SameLine(0.0f, 4.0f);
                    std::string fname = std::filesystem::path(ls.ScriptPath).filename().string();
                    ImGui::TextDisabled("%s", fname.c_str());
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", ls.ScriptPath.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("\xc3\x97##ClearScript")) { // UTF-8 ×
                        ls.ScriptPath.clear();
                        is_modified = true;
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Clear script path");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Reload##LuaReload")) {
                        auto full = Loom::Project::GetAssetFileSystemPath(ls.ScriptPath);
                        Loom::ScriptingEngine::OnFileChanged(full.generic_string());
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Hot-reload script (only active during Play)");
                }

                // --- Script Properties ---
                if (!ls.ScriptPath.empty()) {
                    auto schema = Loom::ScriptingEngine::GetScriptFields(ls.ScriptPath);
                    if (!schema.empty()) {
                        ImGui::Separator();
                        ImGui::TextDisabled("Script Properties");
                        ImGui::Spacing();

                        for (const auto& schema_field : schema) {
                            // Start from schema default; apply stored override only if types match.
                            Loom::ScriptField display_field = schema_field;
                            auto ov_it = ls.Fields.find(schema_field.Name);
                            if (ov_it != ls.Fields.end() && ov_it->second.Type == schema_field.Type)
                                display_field = ov_it->second;

                            // In play mode, try to read the live runtime value.
                            if (mIsPlayMode) {
                                Loom::ScriptField live = schema_field;
                                if (Loom::ScriptingEngine::TryGetFieldValue(entity, schema_field.Name, live))
                                    display_field = live;
                            }

                            ImGui::PushID(schema_field.Name.c_str());
                            ImGui::BeginDisabled(mIsPlayMode);

                            bool field_modified = false;
                            switch (schema_field.Type) {
                                case Loom::ScriptFieldType::Float: {
                                    float v = std::get<float>(display_field.Value);
                                    if (ImGui::DragFloat(schema_field.Name.c_str(), &v, 0.1f)) {
                                        ls.Fields[schema_field.Name] = { schema_field.Name, schema_field.Type, v };
                                        field_modified = true;
                                    }
                                    break;
                                }
                                case Loom::ScriptFieldType::Int: {
                                    int v = std::get<int>(display_field.Value);
                                    if (ImGui::DragInt(schema_field.Name.c_str(), &v)) {
                                        ls.Fields[schema_field.Name] = { schema_field.Name, schema_field.Type, v };
                                        field_modified = true;
                                    }
                                    break;
                                }
                                case Loom::ScriptFieldType::Bool: {
                                    bool v = std::get<bool>(display_field.Value);
                                    if (ImGui::Checkbox(schema_field.Name.c_str(), &v)) {
                                        ls.Fields[schema_field.Name] = { schema_field.Name, schema_field.Type, v };
                                        field_modified = true;
                                    }
                                    break;
                                }
                                case Loom::ScriptFieldType::Vec2: {
                                    glm::vec2 v = std::get<glm::vec2>(display_field.Value);
                                    if (ImGui::DragFloat2(schema_field.Name.c_str(), glm::value_ptr(v), 0.1f)) {
                                        ls.Fields[schema_field.Name] = { schema_field.Name, schema_field.Type, v };
                                        field_modified = true;
                                    }
                                    break;
                                }
                                case Loom::ScriptFieldType::Vec3: {
                                    glm::vec3 v = std::get<glm::vec3>(display_field.Value);
                                    if (ImGui::DragFloat3(schema_field.Name.c_str(), glm::value_ptr(v), 0.1f)) {
                                        ls.Fields[schema_field.Name] = { schema_field.Name, schema_field.Type, v };
                                        field_modified = true;
                                    }
                                    break;
                                }
                                case Loom::ScriptFieldType::String: {
                                    std::string sv = std::get<std::string>(display_field.Value);
                                    char buf[256] = {};
                                    strncpy(buf, sv.c_str(), sizeof(buf) - 1);
                                    if (ImGui::InputText(schema_field.Name.c_str(), buf, sizeof(buf))) {
                                        ls.Fields[schema_field.Name] = { schema_field.Name, schema_field.Type, std::string(buf) };
                                        field_modified = true;
                                    }
                                    break;
                                }
                            }

                            ImGui::EndDisabled();
                            ImGui::PopID();
                            is_modified |= field_modified;
                        }
                    }
                }

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::LuaScriptComponent>("Lua Script");
        }

        if (entity.HasComponent<Loom::AnimationComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::AnimationComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Sprite Animator");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& anim        = entity.GetComponent<Loom::AnimationComponent>();
                bool  is_modified = false;

                is_modified |= ImGui::DragFloat("Frame Duration", &anim.FrameDuration, 0.01f, 0.001f, 60.0f);
                is_modified |= ImGui::Checkbox("Loop",       &anim.Loop);
                ImGui::SameLine();
                is_modified |= ImGui::Checkbox("Playing",    &anim.IsPlaying);

                ImGui::Text("Frames (%d)", (int)anim.Frames.size());
                ImGui::SameLine();
                if (ImGui::SmallButton("+##AddFrame")) {
                    anim.Frames.push_back({ 0.0f, 0.0f, 1.0f, 1.0f });
                    is_modified = true;
                }

                for (int i = 0; i < (int)anim.Frames.size(); i++) {
                    ImGui::PushID(i);
                    auto& frame = anim.Frames[i];
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 24.0f);
                    char label[16];
                    snprintf(label, sizeof(label), "[%d]", i);
                    is_modified |= ImGui::DragFloat4(label, &frame.x, 0.01f, 0.0f, 1.0f);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x##RemoveFrame")) {
                        anim.Frames.erase(anim.Frames.begin() + i);
                        if (anim.CurrentFrame >= (int)anim.Frames.size())
                            anim.CurrentFrame = (int)anim.Frames.size() - 1;
                        is_modified = true;
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }

                ImGui::Separator();
                if (ImGui::CollapsingHeader("Generate from Spritesheet")) {
                    static int ss_sheet_w     = 512;
                    static int ss_sheet_h     = 512;
                    static int ss_cell_w      = 64;
                    static int ss_cell_h      = 64;
                    static int ss_start_col   = 0;
                    static int ss_start_row   = 0;
                    static int ss_frame_count = 8;

                    ImGui::InputInt("Sheet Width (px)",  &ss_sheet_w);
                    ImGui::InputInt("Sheet Height (px)", &ss_sheet_h);
                    ImGui::InputInt("Cell Width (px)",   &ss_cell_w);
                    ImGui::InputInt("Cell Height (px)",  &ss_cell_h);
                    ImGui::InputInt("Start Column",      &ss_start_col);
                    ImGui::InputInt("Start Row",         &ss_start_row);
                    ImGui::InputInt("Frame Count",       &ss_frame_count);

                    ss_sheet_w     = std::max(1, ss_sheet_w);
                    ss_sheet_h     = std::max(1, ss_sheet_h);
                    ss_cell_w      = std::max(1, ss_cell_w);
                    ss_cell_h      = std::max(1, ss_cell_h);
                    ss_start_col   = std::max(0, ss_start_col);
                    ss_start_row   = std::max(0, ss_start_row);
                    ss_frame_count = std::max(1, ss_frame_count);

                    int cols_per_row = ss_sheet_w / ss_cell_w;
                    int rows_total   = ss_sheet_h / ss_cell_h;
                    bool valid = cols_per_row > 0 && rows_total > 0;

                    if (!valid)
                        ImGui::TextColored({ 1.0f, 0.4f, 0.4f, 1.0f }, "Cell size exceeds sheet size.");

                    ImGui::BeginDisabled(!valid);
                    if (ImGui::Button("Generate")) {
                        anim.Frames.clear();
                        float inv_w = 1.0f / (float)ss_sheet_w;
                        float inv_h = 1.0f / (float)ss_sheet_h;
                        for (int i = 0; i < ss_frame_count; i++) {
                            int linear = ss_start_col + ss_start_row * cols_per_row + i;
                            int col    = linear % cols_per_row;
                            int row    = linear / cols_per_row;
                            float u0 = (float)(col * ss_cell_w)       * inv_w;
                            float u1 = (float)((col + 1) * ss_cell_w) * inv_w;
                            // V is flipped: stbi loads with flip, so V=0 is bottom of image;
                            // spritesheet row 0 is at the top (high V).
                            float v0 = 1.0f - (float)((row + 1) * ss_cell_h) * inv_h;
                            float v1 = 1.0f - (float)(row * ss_cell_h)       * inv_h;
                            anim.Frames.push_back({ u0, v0, u1, v1 });
                        }
                        anim.CurrentFrame = 0;
                        is_modified = true;
                    }
                    ImGui::EndDisabled();

                    if (valid) {
                        int last_linear = ss_start_col + ss_start_row * cols_per_row + ss_frame_count - 1;
                        if (last_linear / cols_per_row >= rows_total)
                            ImGui::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "Warning: some frames exceed sheet bounds.");
                    }
                }

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::AnimationComponent>("Sprite Animator");
        }

        if (entity.HasComponent<Loom::AudioSourceComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::AudioSourceComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Audio Source");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& asc         = entity.GetComponent<Loom::AudioSourceComponent>();
                bool  is_modified = false;

                // Asset path row: label | input (fill) | browse button
                char buffer[512] = {};
                strncpy(buffer, asc.AssetPath.c_str(), sizeof(buffer) - 1);

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Audio Clip");
                ImGui::SameLine();
                constexpr float browse_w = 28.0f;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browse_w - ImGui::GetStyle().ItemSpacing.x);
                if (ImGui::InputText("##AudioPath", buffer, sizeof(buffer))) {
                    asc.AssetPath = buffer;
                    is_modified   = true;
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::filesystem::path dropped((const char*)payload->Data);
                        auto ext = dropped.extension();
                        if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac") {
                            asc.AssetPath = dropped.generic_string();
                            is_modified   = true;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine();
                if (ImGui::Button("...##AudioBrowse", { browse_w, 0.0f })) {
                    auto uuid     = entity.GetComponent<Loom::IDComponent>().ID;
                    auto scene    = mContext;
                    auto modified = mSceneModifiedCallback;
                    FileDialog::Open("BrowseAudio", "Choose Audio", ".wav,.mp3,.ogg,.flac",
                        [uuid, scene, modified](const std::string& abs_path) {
                            Loom::Entity e = scene->GetEntityByUUID(uuid);
                            if (!e || !e.HasComponent<Loom::AudioSourceComponent>()) return;
                            e.GetComponent<Loom::AudioSourceComponent>().AssetPath = FileDialog::MakeAssetRelative(abs_path);
                            if (modified) modified();
                        });
                }

                is_modified |= ImGui::SliderFloat("Volume", &asc.Volume, 0.0f, 1.0f);
                is_modified |= ImGui::SliderFloat("Pitch",  &asc.Pitch,  0.1f, 4.0f);
                is_modified |= ImGui::SliderFloat("Pan",    &asc.Pan,   -1.0f, 1.0f);
                is_modified |= ImGui::Checkbox("Loop", &asc.Loop);
                ImGui::SameLine();
                is_modified |= ImGui::Checkbox("Auto Play", &asc.AutoPlay);

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::AudioSourceComponent>("Audio Source");
        }

        if (entity.HasComponent<Loom::TextComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::TextComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Text");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& tc       = entity.GetComponent<Loom::TextComponent>();
                bool  is_modified = false;

                // Font path row: label | input (fill) | browse button
                char font_buffer[512] = {};
                strncpy(font_buffer, tc.FontPath.c_str(), sizeof(font_buffer) - 1);

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Font");
                ImGui::SameLine();
                constexpr float browse_w = 28.0f;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browse_w - ImGui::GetStyle().ItemSpacing.x);
                if (ImGui::InputText("##FontPath", font_buffer, sizeof(font_buffer))) {
                    tc.FontPath = font_buffer;
                    is_modified = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("...##FontBrowse", { browse_w, 0.0f })) {
                    auto uuid     = entity.GetComponent<Loom::IDComponent>().ID;
                    auto scene    = mContext;
                    auto modified = mSceneModifiedCallback;
                    FileDialog::Open("BrowseFont", "Choose Font", ".ttf,.otf",
                        [uuid, scene, modified](const std::string& abs_path) {
                            Loom::Entity e = scene->GetEntityByUUID(uuid);
                            if (!e || !e.HasComponent<Loom::TextComponent>()) return;
                            e.GetComponent<Loom::TextComponent>().FontPath = FileDialog::MakeAssetRelative(abs_path);
                            if (modified) modified();
                        });
                }

                // Text content (multiline)
                char text_buffer[2048] = {};
                strncpy(text_buffer, tc.Text.c_str(), sizeof(text_buffer) - 1);
                ImGui::TextUnformatted("Text");
                if (ImGui::InputTextMultiline("##TextContent", text_buffer, sizeof(text_buffer), { -1.0f, 80.0f })) {
                    tc.Text     = text_buffer;
                    is_modified = true;
                }

                is_modified |= ImGui::ColorEdit4("Color",        glm::value_ptr(tc.Color));
                is_modified |= ImGui::DragFloat("Font Size",    &tc.FontSize,    0.01f,  0.01f, 100.0f, "%.2f");
                is_modified |= ImGui::DragFloat("Kerning",      &tc.Kerning,     0.005f, -1.0f, 5.0f);
                is_modified |= ImGui::DragFloat("Line Spacing", &tc.LineSpacing, 0.005f, -1.0f, 5.0f);

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::TextComponent>("Text");
        }

        if (entity.HasComponent<Loom::TilemapComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::TilemapComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Tilemap");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& tm      = entity.GetComponent<Loom::TilemapComponent>();
                bool is_modified = false;

                // Ensure Tiles is always correctly sized (guards freshly-added components)
                int expected = tm.Columns * tm.Rows;
                if ((int)tm.Tiles.size() != expected) {
                    tm.Tiles.resize(expected, -1);
                    is_modified = true;
                }

                // Spritesheet path row
                char ss_buffer[512] = {};
                strncpy(ss_buffer, tm.SpritesheetPath.c_str(), sizeof(ss_buffer) - 1);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Spritesheet");
                ImGui::SameLine();
                constexpr float browse_w = 28.0f;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browse_w - ImGui::GetStyle().ItemSpacing.x);
                if (ImGui::InputText("##TMSSPath", ss_buffer, sizeof(ss_buffer))) {
                    tm.SpritesheetPath = ss_buffer;
                    tm.Spritesheet     = nullptr;
                    is_modified        = true;
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::filesystem::path picked = std::filesystem::path((const char*)payload->Data);
                        std::filesystem::path asset_dir = Loom::Project::GetAssetDirectory();
                        std::error_code       ec;
                        auto rel = std::filesystem::relative(picked, asset_dir, ec);
                        tm.SpritesheetPath = (!ec && !rel.empty() && rel.string().find("..") == std::string::npos)
                                           ? rel.generic_string() : picked.generic_string();
                        tm.Spritesheet = nullptr;
                        is_modified    = true;
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine();
                if (ImGui::Button("...##TMSSBrowse", { browse_w, 0.0f })) {
                    auto uuid     = entity.GetComponent<Loom::IDComponent>().ID;
                    auto scene    = mContext;
                    auto modified = mSceneModifiedCallback;
                    FileDialog::Open("BrowseTilemapSheet", "Choose Spritesheet", ".png,.jpg,.jpeg,.bmp,.tga",
                        [uuid, scene, modified](const std::string& abs_path) {
                            Loom::Entity e = scene->GetEntityByUUID(uuid);
                            if (!e || !e.HasComponent<Loom::TilemapComponent>()) return;
                            auto& t = e.GetComponent<Loom::TilemapComponent>();
                            t.SpritesheetPath = FileDialog::MakeAssetRelative(abs_path);
                            t.Spritesheet     = nullptr;
                            if (modified) modified();
                        });
                }

                // Grid dimensions
                int grid[2] = { tm.Columns, tm.Rows };
                if (ImGui::DragInt2("Grid (W x H)", grid, 1, 1, 256)) {
                    tm.Columns = std::max(1, grid[0]);
                    tm.Rows    = std::max(1, grid[1]);
                    tm.Tiles.resize(tm.Columns * tm.Rows, -1);
                    is_modified = true;
                }

                // Tile size
                float tile_sz[2] = { tm.TileWidth, tm.TileHeight };
                if (ImGui::DragFloat2("Tile Size", tile_sz, 0.05f, 0.01f, 64.0f, "%.2f")) {
                    tm.TileWidth  = std::max(0.01f, tile_sz[0]);
                    tm.TileHeight = std::max(0.01f, tile_sz[1]);
                    is_modified   = true;
                }

                // Spritesheet layout
                int sheet[2] = { tm.SheetColumns, tm.SheetRows };
                if (ImGui::DragInt2("Sheet (cols x rows)", sheet, 1, 1, 64)) {
                    tm.SheetColumns = std::max(1, sheet[0]);
                    tm.SheetRows    = std::max(1, sheet[1]);
                    is_modified     = true;
                }

                // Tile painter
                if (ImGui::CollapsingHeader("Tile Painter", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static int s_paint_index = 0;
                    int total_sheet_tiles = tm.SheetColumns * tm.SheetRows;

                    ImGui::Text("Paint tile: %d", s_paint_index);
                    ImGui::SameLine();
                    if (ImGui::ArrowButton("##TMPrev", ImGuiDir_Left))
                        s_paint_index = (s_paint_index - 1 + total_sheet_tiles) % total_sheet_tiles;
                    ImGui::SameLine();
                    if (ImGui::ArrowButton("##TMNext", ImGuiDir_Right))
                        s_paint_index = (s_paint_index + 1) % total_sheet_tiles;
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Erase")) s_paint_index = -1;

                    constexpr float cell_sz  = 20.0f;
                    float canvas_w  = (float)tm.Columns * cell_sz;
                    float canvas_h  = (float)tm.Rows    * cell_sz;
                    float avail_w   = ImGui::GetContentRegionAvail().x;
                    float scroll_h  = (canvas_w > avail_w) ? ImGui::GetStyle().ScrollbarSize : 0.0f;
                    float child_h   = std::min(canvas_h, 200.0f) + scroll_h;
                    ImGui::BeginChild("##TileGrid", { 0.0f, child_h },
                                      false, ImGuiWindowFlags_HorizontalScrollbar);

                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    ImVec2      origin    = ImGui::GetCursorScreenPos();
                    ImGui::InvisibleButton("##GridCanvas", { canvas_w, canvas_h });

                    bool   grid_hovered = ImGui::IsItemHovered();
                    ImVec2 mouse_pos    = ImGui::GetMousePos();

                    for (int row = 0; row < tm.Rows; ++row) {
                        for (int col = 0; col < tm.Columns; ++col) {
                            int    idx      = tm.Tiles[row * tm.Columns + col];
                            ImVec2 cell_min = { origin.x + col * cell_sz, origin.y + row * cell_sz };
                            ImVec2 cell_max = { cell_min.x + cell_sz,     cell_min.y + cell_sz };

                            ImU32 bg = (idx < 0) ? IM_COL32(40, 40, 40, 255)
                                                 : IM_COL32(60, 120, 200, 255);
                            draw_list->AddRectFilled(cell_min, cell_max, bg);
                            draw_list->AddRect(cell_min, cell_max, IM_COL32(100, 100, 100, 200));

                            if (idx >= 0) {
                                char label[8];
                                snprintf(label, sizeof(label), "%d", idx);
                                draw_list->AddText({ cell_min.x + 2.0f, cell_min.y + 3.0f },
                                                   IM_COL32(255, 255, 255, 255), label);
                            }

                            if (grid_hovered && ImGui::IsMouseDown(0)) {
                                if (mouse_pos.x >= cell_min.x && mouse_pos.x < cell_max.x &&
                                    mouse_pos.y >= cell_min.y && mouse_pos.y < cell_max.y) {
                                    int& tile = tm.Tiles[row * tm.Columns + col];
                                    if (tile != s_paint_index) {
                                        tile        = s_paint_index;
                                        is_modified = true;
                                    }
                                }
                            }
                        }
                    }

                    ImGui::EndChild();

                    if (ImGui::Button("Clear All##TilemapClear")) {
                        std::fill(tm.Tiles.begin(), tm.Tiles.end(), -1);
                        is_modified = true;
                    }
                }

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::TilemapComponent>("Tilemap");
        }

        if (entity.HasComponent<Loom::ParticleComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::ParticleComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Particles");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& pc          = entity.GetComponent<Loom::ParticleComponent>();
                bool  is_modified = false;

                ImGui::TextDisabled("Live: %d / %d", (int)pc.Live.size(), pc.MaxParticles);

                // Emitter shape
                const char* shape_names[] = { "Point", "Box", "Circle" };
                int shape_idx = (int)pc.Shape;
                if (ImGui::Combo("Shape", &shape_idx, shape_names, IM_ARRAYSIZE(shape_names))) {
                    pc.Shape    = (Loom::ParticleComponent::EmitterShape)shape_idx;
                    is_modified = true;
                }
                if (pc.Shape == Loom::ParticleComponent::EmitterShape::Box) {
                    is_modified |= ImGui::DragFloat2("Box Half-Extents", glm::value_ptr(pc.ShapeSize), 0.05f, 0.0f, 100.0f);
                } else if (pc.Shape == Loom::ParticleComponent::EmitterShape::Circle) {
                    is_modified |= ImGui::DragFloat("Circle Radius", &pc.ShapeSize.x, 0.05f, 0.0f, 100.0f);
                }

                // Simulation space
                const char* space_names[] = { "World", "Local" };
                int space_idx = (int)pc.Space;
                if (ImGui::Combo("Space", &space_idx, space_names, IM_ARRAYSIZE(space_names))) {
                    pc.Space    = (Loom::ParticleComponent::SimulationSpace)space_idx;
                    is_modified = true;
                }

                is_modified |= ImGui::Checkbox("Emitting",   &pc.Emitting);
                is_modified |= ImGui::DragFloat("Spawn Rate", &pc.SpawnRate, 1.0f, 0.0f, 10000.0f, "%.1f /s");

                ImGui::SeparatorText("Particle");

                // Lifetime range as one DragFloat2 (min, max) with min<=max guard
                float life_range[2] = { pc.LifetimeMin, pc.LifetimeMax };
                if (ImGui::DragFloat2("Lifetime (min, max)", life_range, 0.01f, 0.001f, 60.0f, "%.3f")) {
                    pc.LifetimeMin = std::max(0.001f, life_range[0]);
                    pc.LifetimeMax = std::max(pc.LifetimeMin, life_range[1]);
                    is_modified    = true;
                }

                is_modified |= ImGui::DragFloat2("Velocity Min", glm::value_ptr(pc.VelocityMin), 0.05f);
                is_modified |= ImGui::DragFloat2("Velocity Max", glm::value_ptr(pc.VelocityMax), 0.05f);
                is_modified |= ImGui::DragFloat2("Gravity",      glm::value_ptr(pc.Gravity),     0.05f);
                is_modified |= ImGui::DragFloat ("Gravity Scale", &pc.GravityScale, 0.01f,  0.0f, 10.0f);
                is_modified |= ImGui::DragFloat ("Rotation Speed", &pc.RotationSpeed, 0.05f, -50.0f, 50.0f, "%.2f rad/s");

                ImGui::SeparatorText("Appearance");

                is_modified |= ImGui::ColorEdit4("Color Begin", glm::value_ptr(pc.ColorBegin));
                is_modified |= ImGui::ColorEdit4("Color End",   glm::value_ptr(pc.ColorEnd));
                is_modified |= ImGui::DragFloat ("Size Begin",  &pc.SizeBegin, 0.005f, 0.0f, 100.0f, "%.3f");
                is_modified |= ImGui::DragFloat ("Size End",    &pc.SizeEnd,   0.005f, 0.0f, 100.0f, "%.3f");

                if (ImGui::DragInt("Max Particles", &pc.MaxParticles, 1.0f, 1, 65536)) {
                    pc.MaxParticles = std::max(1, pc.MaxParticles);
                    is_modified     = true;
                }

                // Texture row (optional — empty path = plain colored quads)
                char tex_buffer[512] = {};
                strncpy(tex_buffer, pc.TexturePath.c_str(), sizeof(tex_buffer) - 1);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Texture");
                ImGui::SameLine();
                constexpr float browse_w = 28.0f;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browse_w - ImGui::GetStyle().ItemSpacing.x);
                if (ImGui::InputText("##ParticleTexPath", tex_buffer, sizeof(tex_buffer))) {
                    pc.TexturePath = tex_buffer;
                    pc.Texture     = nullptr;
                    is_modified    = true;
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::filesystem::path picked    = std::filesystem::path((const char*)payload->Data);
                        std::filesystem::path asset_dir = Loom::Project::GetAssetDirectory();
                        std::error_code       ec;
                        auto rel = std::filesystem::relative(picked, asset_dir, ec);
                        pc.TexturePath = (!ec && !rel.empty() && rel.string().find("..") == std::string::npos)
                                       ? rel.generic_string() : picked.generic_string();
                        pc.Texture     = nullptr;
                        is_modified    = true;
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine();
                if (ImGui::Button("...##ParticleTexBrowse", { browse_w, 0.0f })) {
                    auto uuid     = entity.GetComponent<Loom::IDComponent>().ID;
                    auto scene    = mContext;
                    auto modified = mSceneModifiedCallback;
                    FileDialog::Open("BrowseParticleTex", "Choose Particle Texture", ".png,.jpg,.jpeg,.bmp,.tga",
                        [uuid, scene, modified](const std::string& abs_path) {
                            Loom::Entity e = scene->GetEntityByUUID(uuid);
                            if (!e || !e.HasComponent<Loom::ParticleComponent>()) return;
                            auto& p = e.GetComponent<Loom::ParticleComponent>();
                            p.TexturePath = FileDialog::MakeAssetRelative(abs_path);
                            p.Texture     = nullptr;
                            if (modified) modified();
                        });
                }

                if (ImGui::Button("Clear Live Particles")) {
                    pc.Live.clear();
                    pc.SpawnAccumulator = 0.0f;
                }

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::ParticleComponent>("Particles");
        }

        if (entity.HasComponent<Loom::MeshRendererComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::MeshRendererComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Mesh Renderer");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& mrc         = entity.GetComponent<Loom::MeshRendererComponent>();
                bool  is_modified = false;

                // ---- Mesh slot --------------------------------------------------
                {
                    std::string mesh_label = mrc.Mesh
                        ? std::filesystem::path(mrc.Mesh->GetPath()).filename().string()
                        : "None (Select...)";

                    ImGui::PushID("MeshSlot");
                    ImGui::TextUnformatted("Mesh");
                    ImGui::SameLine(120);
                    if (ImGui::Button(mesh_label.c_str(), ImVec2(180, 0))) {
                        auto uuid     = entity.GetComponent<Loom::IDComponent>().ID;
                        auto scene    = mContext;
                        auto modified = mSceneModifiedCallback;
                        FileDialog::Open("BrowseMesh", "Choose Mesh", ".glb,.gltf",
                            [uuid, scene, modified](const std::string& abs_path) {
                                Loom::Entity e = scene->GetEntityByUUID(uuid);
                                if (!e || !e.HasComponent<Loom::MeshRendererComponent>()) return;
                                auto new_mesh = Loom::AssetManager::GetMesh(abs_path);
                                if (new_mesh) {
                                    auto& m = e.GetComponent<Loom::MeshRendererComponent>();
                                    m.Mesh     = new_mesh;
                                    m.MeshPath = std::filesystem::relative(abs_path,
                                                    Loom::Project::GetAssetDirectory()).generic_string();
                                    if (modified) modified();
                                }
                            });
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                            std::filesystem::path dropped((const char*)payload->Data);
                            auto ext = dropped.extension();
                            if (ext == ".glb" || ext == ".gltf") {
                                auto full = Loom::Project::GetAssetFileSystemPath(dropped);
                                auto new_mesh = Loom::AssetManager::GetMesh(full.generic_string());
                                if (new_mesh) {
                                    mrc.Mesh     = new_mesh;
                                    mrc.MeshPath = dropped.generic_string();
                                    is_modified = true;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (mrc.Mesh) {
                        ImGui::SameLine();
                        if (ImGui::Button("X##mesh")) {
                            mrc.Mesh.reset();
                            mrc.MeshPath.clear();
                            is_modified = true;
                        }
                        ImGui::TextDisabled("  %u verts / %u indices",
                                            mrc.Mesh->GetVertexCount(), mrc.Mesh->GetIndexCount());
                    }
                    ImGui::PopID();
                }

                ImGui::Separator();

                // ---- Material: albedo color --------------------------------------
                is_modified |= ImGui::ColorEdit4("Albedo", glm::value_ptr(mrc.AlbedoColor));

                // ---- Material: albedo texture ------------------------------------
                {
                    ImTextureID tex_id = (ImTextureID)(uintptr_t)(mrc.AlbedoTexture
                        ? mrc.AlbedoTexture->GetRendererID()
                        : mCheckerboard->GetRendererID());
                    std::string tex_label = mrc.AlbedoTexture
                        ? std::filesystem::path(mrc.AlbedoTexture->GetPath()).filename().string()
                        : "None (Select...)";

                    ImGui::PushID("AlbedoTexSlot");
                    ImGui::Image(tex_id, ImVec2(32, 32), ImVec2(0, 1), ImVec2(1, 0),
                                 ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 0.5f));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                            std::filesystem::path dropped((const char*)payload->Data);
                            auto ext = dropped.extension();
                            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
                                auto full = Loom::Project::GetAssetFileSystemPath(dropped);
                                auto new_tex = Loom::AssetManager::GetTexture(full.generic_string());
                                if (new_tex) {
                                    mrc.AlbedoTexture     = new_tex;
                                    mrc.AlbedoTexturePath = dropped.generic_string();
                                    is_modified = true;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(tex_label.c_str(), ImVec2(150, 0))) {
                        auto uuid     = entity.GetComponent<Loom::IDComponent>().ID;
                        auto scene    = mContext;
                        auto modified = mSceneModifiedCallback;
                        FileDialog::Open("BrowseAlbedoTex", "Choose Albedo Texture", ".png,.jpg,.jpeg,.bmp,.tga",
                            [uuid, scene, modified](const std::string& abs_path) {
                                Loom::Entity e = scene->GetEntityByUUID(uuid);
                                if (!e || !e.HasComponent<Loom::MeshRendererComponent>()) return;
                                auto new_tex = Loom::AssetManager::GetTexture(abs_path);
                                if (new_tex) {
                                    auto& m = e.GetComponent<Loom::MeshRendererComponent>();
                                    m.AlbedoTexture     = new_tex;
                                    m.AlbedoTexturePath = std::filesystem::relative(abs_path,
                                                            Loom::Project::GetAssetDirectory()).generic_string();
                                    if (modified) modified();
                                }
                            });
                    }
                    if (mrc.AlbedoTexture) {
                        ImGui::SameLine();
                        if (ImGui::Button("X##albedotex")) {
                            mrc.AlbedoTexture.reset();
                            mrc.AlbedoTexturePath.clear();
                            is_modified = true;
                        }
                    }
                    ImGui::PopID();
                }

                // ---- Material: surface ------------------------------------------
                is_modified |= ImGui::SliderFloat("Roughness", &mrc.Roughness, 0.0f, 1.0f);
                is_modified |= ImGui::SliderFloat("Metallic",  &mrc.Metallic,  0.0f, 1.0f);

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();

                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::MeshRendererComponent>("Mesh Renderer");
        }

        if (entity.HasComponent<Loom::DirectionalLightComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::DirectionalLightComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Directional Light");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& dl          = entity.GetComponent<Loom::DirectionalLightComponent>();
                bool  is_modified = false;
                is_modified |= ImGui::ColorEdit3("Color",     glm::value_ptr(dl.Color));
                is_modified |= ImGui::DragFloat ("Intensity", &dl.Intensity, 0.05f, 0.0f, 100.0f);
                ImGui::TextDisabled("Direction taken from entity rotation (-Z forward).");

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::DirectionalLightComponent>("Directional Light");
        }

        if (entity.HasComponent<Loom::PointLightComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::PointLightComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Point Light");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& pl          = entity.GetComponent<Loom::PointLightComponent>();
                bool  is_modified = false;
                is_modified |= ImGui::ColorEdit3("Color",     glm::value_ptr(pl.Color));
                is_modified |= ImGui::DragFloat ("Intensity", &pl.Intensity, 0.05f, 0.0f, 100.0f);
                is_modified |= ImGui::DragFloat ("Range",     &pl.Range,     0.1f,  0.01f, 1000.0f);
                ImGui::TextDisabled("Position taken from entity translation.");

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::PointLightComponent>("Point Light");
        }
    }

} // namespace Weaver
