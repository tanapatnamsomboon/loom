#include "scene_hierarchy_panel.h"
#include "editor_context.h"
#include "editor/commands.h"
#include "editor/file_dialog.h"
#include <loom/asset/asset_manager.h>
#include <loom/core/log.h>
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
            if (ImGui::MenuItem("Duplicate Entity", "Ctrl+D")) {
                if (mCommandCallback) {
                    auto cmd = std::make_unique<EntityDuplicateCommand>(mContext, entity);
                    auto* raw = cmd.get();
                    mCommandCallback(std::move(cmd));
                    if (raw->GetNewUUID())
                        mSelectionContext = mContext->GetEntityByUUID(Loom::UUID(raw->GetNewUUID()));
                } else {
                    mSelectionContext = mContext->DuplicateEntity(entity);
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
        // Namespace every widget id below by the entity's UUID. Without this,
        // switching selection mid-edit of an InputText (or any deferred-commit
        // widget) lets ImGui flush the pending buffer into the NEW entity's
        // value because the widget IDs collided across entities.
        ImGui::PushID((const void*)(uintptr_t)(uint64_t)entity_uuid);

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
            push_add.template operator()<Loom::Rigidbody3DComponent>("Rigidbody 3D");
            push_add.template operator()<Loom::BoxCollider3DComponent>("Box Collider 3D");
            push_add.template operator()<Loom::SphereCollider3DComponent>("Sphere Collider 3D");
            push_add.template operator()<Loom::CapsuleCollider3DComponent>("Capsule Collider 3D");
            push_add.template operator()<Loom::LuaScriptComponent>("Lua Script");
            push_add.template operator()<Loom::AnimationComponent>("Sprite Animator");
            push_add.template operator()<Loom::SkeletalAnimationComponent>("Skeletal Animator");
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

                if (ImGui::Checkbox("Fixed Aspect Ratio", &cc.FixedAspectRatio)) {
                    is_modified = true;
                    if (cc.FixedAspectRatio) camera.SetAspectRatio(cc.AspectRatio);
                }
                if (cc.FixedAspectRatio) {
                    if (ImGui::DragFloat("Aspect", &cc.AspectRatio, 0.01f, 0.1f, 10.0f, "%.4f")) {
                        camera.SetAspectRatio(cc.AspectRatio);
                        is_modified = true;
                    }
                    struct AspectPreset { const char* label; float value; };
                    static const AspectPreset presets[] = {
                        { "16:9",  16.0f / 9.0f },
                        { "16:10", 16.0f / 10.0f },
                        { "4:3",   4.0f  / 3.0f  },
                        { "21:9",  21.0f / 9.0f  },
                        { "2:1",   2.0f },
                        { "1:1",   1.0f },
                    };
                    for (size_t i = 0; i < IM_ARRAYSIZE(presets); ++i) {
                        if (i > 0) ImGui::SameLine(0, 4.0f);
                        if (ImGui::SmallButton(presets[i].label)) {
                            cc.AspectRatio = presets[i].value;
                            camera.SetAspectRatio(cc.AspectRatio);
                            is_modified = true;
                        }
                    }
                }

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

                // ---- Playback header: pick active clip, toggle play ----
                const char* current_clip_label = anim.CurrentClip.empty() ? "(none)" : anim.CurrentClip.c_str();
                if (ImGui::BeginCombo("Current Clip", current_clip_label)) {
                    bool none_selected = anim.CurrentClip.empty();
                    if (ImGui::Selectable("(none)", none_selected)) {
                        anim.CurrentClip  = "";
                        anim.CurrentFrame = 0;
                        anim.ElapsedTime  = 0.0f;
                        is_modified = true;
                    }
                    for (auto& clip : anim.Clips) {
                        bool selected = (clip.Name == anim.CurrentClip);
                        if (ImGui::Selectable(clip.Name.c_str(), selected)) {
                            anim.CurrentClip  = clip.Name;
                            anim.CurrentFrame = 0;
                            anim.ElapsedTime  = 0.0f;
                            is_modified = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                is_modified |= ImGui::Checkbox("Playing", &anim.IsPlaying);
                ImGui::SameLine();
                ImGui::TextDisabled("frame %d", anim.CurrentFrame);

                ImGui::Separator();

                // ---- Clip list ----
                ImGui::Text("Clips (%d)", (int)anim.Clips.size());
                ImGui::SameLine();
                if (ImGui::SmallButton("+##AddClip")) {
                    std::string name = "Clip" + std::to_string(anim.Clips.size());
                    anim.Clips.push_back(Loom::AnimationClip(name));
                    if (anim.CurrentClip.empty()) anim.CurrentClip = name;
                    is_modified = true;
                }

                int  remove_clip_index = -1;
                for (int ci = 0; ci < (int)anim.Clips.size(); ci++) {
                    ImGui::PushID(ci);
                    auto& clip = anim.Clips[ci];

                    if (ImGui::CollapsingHeader(clip.Name.empty() ? "(unnamed)" : clip.Name.c_str(),
                                                ImGuiTreeNodeFlags_DefaultOpen)) {
                        // Name input
                        char name_buf[64];
                        std::strncpy(name_buf, clip.Name.c_str(), sizeof(name_buf));
                        name_buf[sizeof(name_buf) - 1] = '\0';
                        if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
                            std::string new_name = name_buf;
                            // If this clip was the active one, keep CurrentClip pointing at it after rename.
                            if (anim.CurrentClip == clip.Name) anim.CurrentClip = new_name;
                            clip.Name = std::move(new_name);
                            is_modified = true;
                        }

                        is_modified |= ImGui::DragFloat("Frame Duration", &clip.FrameDuration, 0.01f, 0.001f, 60.0f);
                        is_modified |= ImGui::Checkbox ("Loop",           &clip.Loop);

                        // Frames list
                        ImGui::Text("Frames (%d)", (int)clip.Frames.size());
                        ImGui::SameLine();
                        if (ImGui::SmallButton("+##AddFrame")) {
                            clip.Frames.push_back({ 0.0f, 0.0f, 1.0f, 1.0f });
                            is_modified = true;
                        }

                        for (int i = 0; i < (int)clip.Frames.size(); i++) {
                            ImGui::PushID(i);
                            auto& frame = clip.Frames[i];
                            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 24.0f);
                            char label[16];
                            snprintf(label, sizeof(label), "[%d]", i);
                            is_modified |= ImGui::DragFloat4(label, &frame.x, 0.01f, 0.0f, 1.0f);
                            ImGui::SameLine();
                            if (ImGui::SmallButton("x##RemoveFrame")) {
                                clip.Frames.erase(clip.Frames.begin() + i);
                                if (clip.Name == anim.CurrentClip && anim.CurrentFrame >= (int)clip.Frames.size())
                                    anim.CurrentFrame = std::max(0, (int)clip.Frames.size() - 1);
                                is_modified = true;
                                ImGui::PopID();
                                break;
                            }
                            ImGui::PopID();
                        }

                        // Events list — frame index + named tag delivered to Lua's OnAnimationEvent(name).
                        ImGui::Spacing();
                        ImGui::Text("Events (%d)", (int)clip.Events.size());
                        ImGui::SameLine();
                        if (ImGui::SmallButton("+##AddEvent")) {
                            clip.Events.push_back(Loom::AnimationEvent(0, "event"));
                            is_modified = true;
                        }

                        int remove_event_index = -1;
                        int max_frame_idx = std::max(0, (int)clip.Frames.size() - 1);
                        for (int ei = 0; ei < (int)clip.Events.size(); ei++) {
                            ImGui::PushID(ei + 10000); // offset to avoid collision with frame IDs
                            auto& ev = clip.Events[ei];

                            ImGui::SetNextItemWidth(60.0f);
                            is_modified |= ImGui::DragInt("##EventFrame", &ev.Frame, 0.1f, 0, max_frame_idx);
                            ImGui::SameLine();

                            char name_buf[128];
                            std::strncpy(name_buf, ev.Name.c_str(), sizeof(name_buf));
                            name_buf[sizeof(name_buf) - 1] = '\0';
                            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 24.0f);
                            if (ImGui::InputText("##EventName", name_buf, sizeof(name_buf))) {
                                ev.Name = name_buf;
                                is_modified = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("x##RemoveEvent")) {
                                remove_event_index = ei;
                            }
                            ImGui::PopID();
                        }
                        if (remove_event_index >= 0) {
                            clip.Events.erase(clip.Events.begin() + remove_event_index);
                            is_modified = true;
                        }

                        // Visual spritesheet picker — click cells to add/remove them from this
                        // clip's frame list. Selection order = playback order. Cells already in
                        // the frame list are highlighted with their playback index.
                        if (ImGui::TreeNodeEx("Spritesheet Picker", ImGuiTreeNodeFlags_DefaultOpen)) {
                            std::shared_ptr<Loom::Texture2D> tex;
                            if (entity.HasComponent<Loom::SpriteRendererComponent>())
                                tex = entity.GetComponent<Loom::SpriteRendererComponent>().Texture;

                            if (!tex) {
                                ImGui::TextColored({ 1.0f, 0.4f, 0.4f, 1.0f },
                                    "Assign a texture to Sprite Renderer first.");
                            } else {
                                int sheet_w = (int)tex->GetWidth();
                                int sheet_h = (int)tex->GetHeight();

                                ImGui::SetNextItemWidth(120.0f);
                                is_modified |= ImGui::InputInt("Cell W (px)", &anim.PickerCellWidth);
                                ImGui::SameLine();
                                ImGui::SetNextItemWidth(120.0f);
                                is_modified |= ImGui::InputInt("Cell H (px)", &anim.PickerCellHeight);
                                anim.PickerCellWidth  = std::clamp(anim.PickerCellWidth,  1, sheet_w);
                                anim.PickerCellHeight = std::clamp(anim.PickerCellHeight, 1, sheet_h);

                                int cols_total = sheet_w / anim.PickerCellWidth;
                                int rows_total = sheet_h / anim.PickerCellHeight;

                                if (cols_total <= 0 || rows_total <= 0) {
                                    ImGui::TextColored({ 1.0f, 0.4f, 0.4f, 1.0f },
                                        "Cell size exceeds sheet size.");
                                } else {
                                    ImGui::TextDisabled(
                                        "Sheet %dx%d   Grid %dx%d   (click to add/remove)",
                                        sheet_w, sheet_h, cols_total, rows_total);

                                    float content_w = ImGui::GetContentRegionAvail().x;
                                    float display_w = std::min(content_w, 480.0f);
                                    float aspect    = (float)sheet_h / (float)sheet_w;
                                    float display_h = display_w * aspect;

                                    ImGui::Image((ImTextureID)(uintptr_t)tex->GetRendererID(),
                                                 ImVec2(display_w, display_h),
                                                 ImVec2(0, 1), ImVec2(1, 0));
                                    ImVec2 img_min = ImGui::GetItemRectMin();
                                    ImVec2 img_max = ImGui::GetItemRectMax();
                                    bool   img_hovered = ImGui::IsItemHovered();

                                    ImDrawList* dl = ImGui::GetWindowDrawList();
                                    float cell_px_w = display_w / (float)cols_total;
                                    float cell_px_h = display_h / (float)rows_total;

                                    // Grid lines
                                    for (int c = 0; c <= cols_total; c++) {
                                        float x = img_min.x + c * cell_px_w;
                                        dl->AddLine(ImVec2(x, img_min.y), ImVec2(x, img_max.y),
                                                    IM_COL32(255, 255, 255, 60));
                                    }
                                    for (int r = 0; r <= rows_total; r++) {
                                        float y = img_min.y + r * cell_px_h;
                                        dl->AddLine(ImVec2(img_min.x, y), ImVec2(img_max.x, y),
                                                    IM_COL32(255, 255, 255, 60));
                                    }

                                    // Map a frame's UV back to a (col, row) cell. Inverse of the
                                    // cell -> UV math below — uses cell midpoint to be robust to
                                    // float rounding.
                                    auto uv_to_cell = [&](const glm::vec4& uv, int& out_c, int& out_r) -> bool {
                                        float u_mid = (uv.x + uv.z) * 0.5f;
                                        float v_mid = (uv.y + uv.w) * 0.5f;
                                        out_c = (int)std::floor(u_mid * cols_total);
                                        out_r = (int)std::floor((1.0f - v_mid) * rows_total);
                                        return out_c >= 0 && out_c < cols_total
                                            && out_r >= 0 && out_r < rows_total;
                                    };

                                    // Highlight cells that are already frames; mark the currently
                                    // playing frame separately so you can see playback advance.
                                    for (int fi = 0; fi < (int)clip.Frames.size(); fi++) {
                                        int c, r;
                                        if (!uv_to_cell(clip.Frames[fi], c, r)) continue;
                                        ImVec2 cmin(img_min.x + c * cell_px_w, img_min.y + r * cell_px_h);
                                        ImVec2 cmax(cmin.x + cell_px_w, cmin.y + cell_px_h);
                                        bool is_active = (clip.Name == anim.CurrentClip
                                                          && fi == anim.CurrentFrame);
                                        ImU32 fill = is_active
                                                   ? IM_COL32(255, 220, 100, 110)
                                                   : IM_COL32( 80, 200, 120,  80);
                                        dl->AddRectFilled(cmin, cmax, fill);
                                        dl->AddRect      (cmin, cmax, IM_COL32(255, 255, 255, 180));
                                        char num[8];
                                        snprintf(num, sizeof(num), "%d", fi);
                                        dl->AddText(ImVec2(cmin.x + 2.0f, cmin.y + 2.0f),
                                                    IM_COL32(255, 255, 255, 220), num);
                                    }

                                    // Hover outline + click toggle
                                    if (img_hovered) {
                                        ImVec2 mp = ImGui::GetMousePos();
                                        int hc = (int)((mp.x - img_min.x) / cell_px_w);
                                        int hr = (int)((mp.y - img_min.y) / cell_px_h);
                                        if (hc >= 0 && hc < cols_total && hr >= 0 && hr < rows_total) {
                                            ImVec2 hmin(img_min.x + hc * cell_px_w,
                                                        img_min.y + hr * cell_px_h);
                                            ImVec2 hmax(hmin.x + cell_px_w, hmin.y + cell_px_h);
                                            dl->AddRect(hmin, hmax,
                                                        IM_COL32(255, 200, 80, 220), 0.0f, 0, 2.0f);

                                            if (ImGui::IsMouseClicked(0)) {
                                                int existing = -1;
                                                for (int fi = 0; fi < (int)clip.Frames.size(); fi++) {
                                                    int c, r;
                                                    if (uv_to_cell(clip.Frames[fi], c, r)
                                                        && c == hc && r == hr) {
                                                        existing = fi;
                                                        break;
                                                    }
                                                }
                                                if (existing >= 0) {
                                                    clip.Frames.erase(clip.Frames.begin() + existing);
                                                    if (clip.Name == anim.CurrentClip
                                                        && anim.CurrentFrame >= (int)clip.Frames.size())
                                                        anim.CurrentFrame = std::max(0, (int)clip.Frames.size() - 1);
                                                } else {
                                                    // V is flipped (stbi loads with flip): row 0 of the
                                                    // sheet sits at high V; mirror that here.
                                                    float inv_w = 1.0f / (float)sheet_w;
                                                    float inv_h = 1.0f / (float)sheet_h;
                                                    float u0 = (float)(hc       * anim.PickerCellWidth)  * inv_w;
                                                    float u1 = (float)((hc + 1) * anim.PickerCellWidth)  * inv_w;
                                                    float v0 = 1.0f - (float)((hr + 1) * anim.PickerCellHeight) * inv_h;
                                                    float v1 = 1.0f - (float)( hr      * anim.PickerCellHeight) * inv_h;
                                                    clip.Frames.push_back({ u0, v0, u1, v1 });
                                                }
                                                is_modified = true;
                                            }
                                        }
                                    }

                                    if (ImGui::SmallButton("Clear Frames##Picker")) {
                                        clip.Frames.clear();
                                        if (clip.Name == anim.CurrentClip) anim.CurrentFrame = 0;
                                        is_modified = true;
                                    }
                                }
                            }
                            ImGui::TreePop();
                        }

                        if (ImGui::Button("Delete Clip")) {
                            remove_clip_index = ci;
                        }
                    }
                    ImGui::PopID();
                }

                if (remove_clip_index >= 0) {
                    std::string removed_name = anim.Clips[remove_clip_index].Name;
                    anim.Clips.erase(anim.Clips.begin() + remove_clip_index);
                    if (anim.CurrentClip == removed_name) {
                        anim.CurrentClip  = anim.Clips.empty() ? "" : anim.Clips.front().Name;
                        anim.CurrentFrame = 0;
                        anim.ElapsedTime  = 0.0f;
                    }
                    is_modified = true;
                }

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::AnimationComponent>("Sprite Animator");
        }

        if (entity.HasComponent<Loom::SkeletalAnimationComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::SkeletalAnimationComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Skeletal Animator");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& anim       = entity.GetComponent<Loom::SkeletalAnimationComponent>();
                bool is_modified = false;

                // Clip dropdown sources its options from the sibling MeshRendererComponent's
                // mesh asset. Without a skinned mesh, the combo lists "(none)" only and the
                // player is effectively idle.
                std::shared_ptr<Loom::MeshAsset> mesh;
                if (entity.HasComponent<Loom::MeshRendererComponent>())
                    mesh = entity.GetComponent<Loom::MeshRendererComponent>().Mesh;

                const char* current_label = anim.CurrentClip.empty() ? "(none)" : anim.CurrentClip.c_str();
                if (ImGui::BeginCombo("Current Clip", current_label)) {
                    if (ImGui::Selectable("(none)", anim.CurrentClip.empty())) {
                        anim.CurrentClip = "";
                        anim.Time        = 0.0f;
                        is_modified      = true;
                    }
                    if (mesh) {
                        for (const auto& clip : mesh->GetClips()) {
                            bool selected = (clip.Name == anim.CurrentClip);
                            if (ImGui::Selectable(clip.Name.c_str(), selected)) {
                                anim.CurrentClip = clip.Name;
                                anim.Time        = 0.0f;
                                is_modified      = true;
                            }
                        }
                    }
                    ImGui::EndCombo();
                }

                if (!mesh || !mesh->IsSkinned()) {
                    ImGui::TextDisabled("(needs a sibling MeshRenderer with a skinned mesh)");
                }

                is_modified |= ImGui::Checkbox("Playing", &anim.IsPlaying);
                ImGui::SameLine();
                is_modified |= ImGui::Checkbox("Loop", &anim.Loop);
                is_modified |= ImGui::DragFloat("Speed", &anim.Speed, 0.01f, -4.0f, 4.0f);

                // Time scrubber: shows current time / duration. Editable in
                // either play or pause state for manual frame inspection.
                float duration = 0.0f;
                if (mesh) {
                    if (const auto* clip = mesh->FindClip(anim.CurrentClip))
                        duration = clip->Duration;
                }
                if (duration > 0.0f) {
                    is_modified |= ImGui::SliderFloat("Time", &anim.Time, 0.0f, duration, "%.3fs");
                } else {
                    ImGui::TextDisabled("Time: (no clip selected)");
                }

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::SkeletalAnimationComponent>("Skeletal Animator");
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
                // Same defense for the solid flag table — keyed by sheet tile index.
                int sheet_total = tm.SheetColumns * tm.SheetRows;
                if ((int)tm.Solid.size() != sheet_total) {
                    tm.Solid.resize(sheet_total, false);
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
                    // Solid is sheet-tile-indexed; resize in the same frame to keep
                    // the palette read below from running off the old end.
                    tm.Solid.resize(tm.SheetColumns * tm.SheetRows, false);
                    is_modified     = true;
                }

                // Tile painter — shared selected-tile state lives on EditorContext so
                // the viewport paint loop and the inspector grid stay in sync.
                if (ImGui::CollapsingHeader("Tile Painter", ImGuiTreeNodeFlags_DefaultOpen)) {
                    int& selected_tile = mEditorContext ? mEditorContext->SelectedTileIndex
                                                        : *(new int(0)); // safe fallback; ctx is always set in practice
                    int total_sheet_tiles = tm.SheetColumns * tm.SheetRows;

                    // --- Viewport paint mode toggle (B) ---
                    if (mEditorContext) {
                        bool paint_on = (mEditorContext->Tool == ToolMode::TilePaint);
                        ImGui::PushStyleColor(ImGuiCol_Button,
                            paint_on ? ImVec4(0.95f, 0.55f, 0.10f, 1.0f) : ImGui::GetStyle().Colors[ImGuiCol_Button]);
                        if (ImGui::Button(paint_on ? "Paint in Viewport: ON  (B)"
                                                   : "Paint in Viewport: OFF (B)")) {
                            mEditorContext->Tool = paint_on ? ToolMode::Transform : ToolMode::TilePaint;
                        }
                        ImGui::PopStyleColor();
                    }

                    ImGui::Text("Selected tile: %s",
                        (selected_tile < 0) ? "Eraser" : std::to_string(selected_tile).c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Eraser##TM")) selected_tile = -1;
                    ImGui::SameLine();
                    if (ImGui::ArrowButton("##TMPrev", ImGuiDir_Left))
                        selected_tile = (selected_tile - 1 + total_sheet_tiles) % total_sheet_tiles;
                    ImGui::SameLine();
                    if (ImGui::ArrowButton("##TMNext", ImGuiDir_Right))
                        selected_tile = (selected_tile + 1) % total_sheet_tiles;

                    // --- Visual tile palette (uses the loaded spritesheet) ---
                    std::shared_ptr<Loom::Texture2D> sheet_tex;
                    if (!tm.SpritesheetPath.empty()) {
                        auto abs = Loom::Project::GetAssetFileSystemPath(tm.SpritesheetPath).generic_string();
                        if (!tm.Spritesheet || tm.Spritesheet->GetPath() != abs)
                            tm.Spritesheet = Loom::AssetManager::GetTexture(abs);
                        sheet_tex = tm.Spritesheet;
                    }

                    if (sheet_tex && total_sheet_tiles > 0) {
                        ImGui::TextDisabled("Palette  (Click: select   Shift+Click: toggle Solid)");
                        float content_w   = ImGui::GetContentRegionAvail().x;
                        float palette_w   = std::min(content_w, 320.0f);
                        float palette_h   = palette_w * ((float)tm.SheetRows / (float)tm.SheetColumns);
                        ImGui::Image((ImTextureID)(uintptr_t)sheet_tex->GetRendererID(),
                                     ImVec2(palette_w, palette_h),
                                     ImVec2(0, 1), ImVec2(1, 0));
                        ImVec2 pmin = ImGui::GetItemRectMin();
                        ImVec2 pmax = ImGui::GetItemRectMax();
                        bool   p_hovered = ImGui::IsItemHovered();
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        float pcell_w = palette_w / (float)tm.SheetColumns;
                        float pcell_h = palette_h / (float)tm.SheetRows;
                        // Grid lines
                        for (int c = 0; c <= tm.SheetColumns; ++c) {
                            float x = pmin.x + c * pcell_w;
                            dl->AddLine({ x, pmin.y }, { x, pmax.y }, IM_COL32(255, 255, 255, 60));
                        }
                        for (int r = 0; r <= tm.SheetRows; ++r) {
                            float y = pmin.y + r * pcell_h;
                            dl->AddLine({ pmin.x, y }, { pmax.x, y }, IM_COL32(255, 255, 255, 60));
                        }
                        // Red overlay for solid tiles. Border edges are drawn only when the
                        // neighbour in that direction is NOT solid — adjacent solid cells
                        // share an interior edge, so drawing it from both sides would
                        // double the apparent thickness.
                        auto solid_at = [&](int c, int r) -> bool {
                            if (c < 0 || c >= tm.SheetColumns || r < 0 || r >= tm.SheetRows) return false;
                            return tm.Solid[r * tm.SheetColumns + c];
                        };
                        ImU32 solid_fill   = IM_COL32(220, 60, 60, 100);
                        ImU32 solid_border = IM_COL32(220, 60, 60, 220);
                        for (int sr = 0; sr < tm.SheetRows; ++sr) {
                            for (int sc = 0; sc < tm.SheetColumns; ++sc) {
                                if (!tm.Solid[sr * tm.SheetColumns + sc]) continue;
                                ImVec2 smin{ pmin.x + sc * pcell_w, pmin.y + sr * pcell_h };
                                ImVec2 smax{ smin.x + pcell_w,      smin.y + pcell_h };
                                dl->AddRectFilled(smin, smax, solid_fill);
                                if (!solid_at(sc, sr - 1)) // top
                                    dl->AddLine({ smin.x, smin.y }, { smax.x, smin.y }, solid_border, 1.5f);
                                if (!solid_at(sc, sr + 1)) // bottom
                                    dl->AddLine({ smin.x, smax.y }, { smax.x, smax.y }, solid_border, 1.5f);
                                if (!solid_at(sc - 1, sr)) // left
                                    dl->AddLine({ smin.x, smin.y }, { smin.x, smax.y }, solid_border, 1.5f);
                                if (!solid_at(sc + 1, sr)) // right
                                    dl->AddLine({ smax.x, smin.y }, { smax.x, smax.y }, solid_border, 1.5f);
                            }
                        }
                        // Highlight selected
                        if (selected_tile >= 0 && selected_tile < total_sheet_tiles) {
                            int sc = selected_tile % tm.SheetColumns;
                            int sr = selected_tile / tm.SheetColumns;
                            ImVec2 smin{ pmin.x + sc * pcell_w, pmin.y + sr * pcell_h };
                            ImVec2 smax{ smin.x + pcell_w, smin.y + pcell_h };
                            dl->AddRect(smin, smax, IM_COL32(255, 200, 80, 255), 0.0f, 0, 3.0f);
                        }
                        // Hover + click — plain click selects, shift+click toggles Solid.
                        if (p_hovered) {
                            ImVec2 mp = ImGui::GetMousePos();
                            int hc = (int)((mp.x - pmin.x) / pcell_w);
                            int hr = (int)((mp.y - pmin.y) / pcell_h);
                            if (hc >= 0 && hc < tm.SheetColumns && hr >= 0 && hr < tm.SheetRows) {
                                ImVec2 hmin{ pmin.x + hc * pcell_w, pmin.y + hr * pcell_h };
                                ImVec2 hmax{ hmin.x + pcell_w, hmin.y + pcell_h };
                                dl->AddRect(hmin, hmax, IM_COL32(255, 255, 255, 180), 0.0f, 0, 1.5f);
                                if (ImGui::IsMouseClicked(0)) {
                                    int sheet_idx = hr * tm.SheetColumns + hc;
                                    if (ImGui::GetIO().KeyShift) {
                                        tm.Solid[sheet_idx] = !tm.Solid[sheet_idx];
                                        is_modified = true;
                                    } else {
                                        selected_tile = sheet_idx;
                                    }
                                }
                            }
                        }
                    } else if (!sheet_tex) {
                        ImGui::TextColored({ 1.0f, 0.4f, 0.4f, 1.0f },
                            "Assign a Spritesheet to enable the visual palette.");
                    }

                    // --- Inspector tile grid overview (small, clickable) ---
                    ImGui::Spacing();
                    ImGui::TextDisabled("Map (click + drag to paint)");
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

                            bool  is_solid = (idx >= 0 && idx < (int)tm.Solid.size() && tm.Solid[idx]);
                            ImU32 bg = (idx < 0)  ? IM_COL32(40, 40, 40, 255)
                                     : is_solid   ? IM_COL32(180, 60, 60, 255)
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
                                    if (tile != selected_tile) {
                                        tile        = selected_tile;
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

                // ---- Import material from glTF ----------------------------------
                {
                    const bool has_gltf_mat = mrc.Mesh && mrc.Mesh->GetMaterial().HasMaterial;
                    ImGui::BeginDisabled(!has_gltf_mat);
                    if (ImGui::Button("Import Material from glTF")) {
                        // Mutable copy — ExtractEmbeddedTextures fills its URIs
                        // in-place when there's embedded image data.
                        Loom::MeshMaterial gm = mrc.Mesh->GetMaterial();
                        mrc.AlbedoColor   = gm.BaseColorFactor;
                        mrc.Roughness     = gm.RoughnessFactor;
                        mrc.Metallic      = gm.MetallicFactor;
                        mrc.EmissiveFactor = gm.EmissiveFactor; // folds KHR_materials_emissive_strength

                        // Clean-slate the texture slots so a slot the new mesh
                        // doesn't define doesn't keep the previous mesh's
                        // texture, and so a freshly-extracted file at the same
                        // path loads with new pixels (AssetManager cache holds
                        // weak_ptrs — dropping the strong ref forces a reload).
                        mrc.AlbedoTexture.reset();   mrc.AlbedoTexturePath.clear();
                        mrc.ORMTexture.reset();      mrc.ORMTexturePath.clear();
                        mrc.EmissiveTexture.reset(); mrc.EmissiveTexturePath.clear();
                        mrc.NormalTexture.reset();   mrc.NormalTexturePath.clear();

                        // glTF texture URIs are relative to the model file's directory.
                        std::filesystem::path model_dir =
                            std::filesystem::path(mrc.Mesh->GetPath()).parent_path();

                        // Extract embedded textures (.glb buffer-views, data-URIs)
                        // into the model dir as <mesh_basename>_<slot>.<ext>.
                        if (gm.BaseColorEmbedded || gm.ORMEmbedded ||
                            gm.EmissiveEmbedded || gm.NormalEmbedded) {
                            std::string prefix = std::filesystem::path(mrc.Mesh->GetPath())
                                                    .stem().string();
                            Loom::MeshAsset::ExtractEmbeddedTextures(
                                mrc.Mesh->GetPath(), model_dir, prefix, gm);
                        }

                        auto import_tex = [&](const std::string& uri, const char* label,
                                              std::shared_ptr<Loom::Texture2D>& out_tex,
                                              std::string& out_path) {
                            if (uri.empty()) return;
                            std::filesystem::path abs_tex = model_dir / uri;
                            if (std::filesystem::exists(abs_tex)) {
                                if (auto new_tex = Loom::AssetManager::GetTexture(
                                        abs_tex.generic_string(), Loom::kMeshAlbedoTextureSpec)) {
                                    out_tex  = new_tex;
                                    out_path = std::filesystem::relative(
                                        abs_tex, Loom::Project::GetAssetDirectory()).generic_string();
                                }
                            } else {
                                LOOM_CORE_WARN("Import Material: {} texture '{}' not found "
                                               "next to the model - factors imported, texture skipped.",
                                               label, abs_tex.generic_string());
                            }
                        };
                        import_tex(gm.BaseColorTexture, "base-color",
                                   mrc.AlbedoTexture, mrc.AlbedoTexturePath);
                        import_tex(gm.ORMTexture, "ORM",
                                   mrc.ORMTexture, mrc.ORMTexturePath);
                        import_tex(gm.EmissiveTexture, "emissive",
                                   mrc.EmissiveTexture, mrc.EmissiveTexturePath);
                        import_tex(gm.NormalTexture, "normal",
                                   mrc.NormalTexture, mrc.NormalTexturePath);
                        is_modified = true;
                    }
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip(has_gltf_mat
                            ? "Copy baseColor / metallic / roughness from the glTF,\n"
                              "load any external textures, and extract embedded textures\n"
                              "(.glb buffer-view or data-URI) next to the model file."
                            : "Assign a glTF/glb mesh that carries a pbrMetallicRoughness\n"
                              "material to enable import.");
                    }
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
                                auto new_tex = Loom::AssetManager::GetTexture(
                                    full.generic_string(), Loom::kMeshAlbedoTextureSpec);
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
                                auto new_tex = Loom::AssetManager::GetTexture(abs_path, Loom::kMeshAlbedoTextureSpec);
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

                // ---- Material: ORM map (R=AO, G=Roughness, B=Metallic) ----------
                {
                    ImTextureID orm_id = (ImTextureID)(uintptr_t)(mrc.ORMTexture
                        ? mrc.ORMTexture->GetRendererID()
                        : mCheckerboard->GetRendererID());
                    std::string orm_label = mrc.ORMTexture
                        ? std::filesystem::path(mrc.ORMTexture->GetPath()).filename().string()
                        : "None (Select...)";

                    ImGui::PushID("ORMTexSlot");
                    ImGui::Image(orm_id, ImVec2(32, 32), ImVec2(0, 1), ImVec2(1, 0),
                                 ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 0.5f));
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("ORM-packed texture:\nR = Ambient Occlusion (modulates IBL ambient)\nG = Roughness (multiplied by slider)\nB = Metallic  (multiplied by slider)");
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                            std::filesystem::path dropped((const char*)payload->Data);
                            auto ext = dropped.extension();
                            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
                                auto full = Loom::Project::GetAssetFileSystemPath(dropped);
                                auto new_tex = Loom::AssetManager::GetTexture(
                                    full.generic_string(), Loom::kMeshAlbedoTextureSpec);
                                if (new_tex) {
                                    mrc.ORMTexture     = new_tex;
                                    mrc.ORMTexturePath = dropped.generic_string();
                                    is_modified = true;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(orm_label.c_str(), ImVec2(150, 0))) {
                        auto uuid     = entity.GetComponent<Loom::IDComponent>().ID;
                        auto scene    = mContext;
                        auto modified = mSceneModifiedCallback;
                        FileDialog::Open("BrowseORMTex", "Choose ORM Texture (R=AO, G=Rough, B=Metal)",
                            ".png,.jpg,.jpeg,.bmp,.tga",
                            [uuid, scene, modified](const std::string& abs_path) {
                                Loom::Entity e = scene->GetEntityByUUID(uuid);
                                if (!e || !e.HasComponent<Loom::MeshRendererComponent>()) return;
                                auto new_tex = Loom::AssetManager::GetTexture(abs_path, Loom::kMeshAlbedoTextureSpec);
                                if (new_tex) {
                                    auto& m = e.GetComponent<Loom::MeshRendererComponent>();
                                    m.ORMTexture     = new_tex;
                                    m.ORMTexturePath = std::filesystem::relative(abs_path,
                                                            Loom::Project::GetAssetDirectory()).generic_string();
                                    if (modified) modified();
                                }
                            });
                    }
                    if (mrc.ORMTexture) {
                        ImGui::SameLine();
                        if (ImGui::Button("X##ormtex")) {
                            mrc.ORMTexture.reset();
                            mrc.ORMTexturePath.clear();
                            is_modified = true;
                        }
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("ORM (R=AO, G=Rough, B=Metal)");
                    ImGui::PopID();
                }

                // ---- Material: emissive (HDR factor multiplies the texture) ----
                is_modified |= ImGui::ColorEdit3("Emissive", glm::value_ptr(mrc.EmissiveFactor),
                                                 ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
                {
                    ImTextureID em_id = (ImTextureID)(uintptr_t)(mrc.EmissiveTexture
                        ? mrc.EmissiveTexture->GetRendererID()
                        : mCheckerboard->GetRendererID());
                    std::string em_label = mrc.EmissiveTexture
                        ? std::filesystem::path(mrc.EmissiveTexture->GetPath()).filename().string()
                        : "None (Select...)";

                    ImGui::PushID("EmissiveTexSlot");
                    ImGui::Image(em_id, ImVec2(32, 32), ImVec2(0, 1), ImVec2(1, 0),
                                 ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 0.5f));
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Emissive map (glTF: sRGB).\nMultiplied by the Emissive factor above —\nset the factor above 0 to see the texture.");
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                            std::filesystem::path dropped((const char*)payload->Data);
                            auto ext = dropped.extension();
                            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
                                auto full = Loom::Project::GetAssetFileSystemPath(dropped);
                                auto new_tex = Loom::AssetManager::GetTexture(
                                    full.generic_string(), Loom::kMeshAlbedoTextureSpec);
                                if (new_tex) {
                                    mrc.EmissiveTexture     = new_tex;
                                    mrc.EmissiveTexturePath = dropped.generic_string();
                                    is_modified = true;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(em_label.c_str(), ImVec2(150, 0))) {
                        auto uuid     = entity.GetComponent<Loom::IDComponent>().ID;
                        auto scene    = mContext;
                        auto modified = mSceneModifiedCallback;
                        FileDialog::Open("BrowseEmissiveTex", "Choose Emissive Texture",
                            ".png,.jpg,.jpeg,.bmp,.tga",
                            [uuid, scene, modified](const std::string& abs_path) {
                                Loom::Entity e = scene->GetEntityByUUID(uuid);
                                if (!e || !e.HasComponent<Loom::MeshRendererComponent>()) return;
                                auto new_tex = Loom::AssetManager::GetTexture(abs_path, Loom::kMeshAlbedoTextureSpec);
                                if (new_tex) {
                                    auto& m = e.GetComponent<Loom::MeshRendererComponent>();
                                    m.EmissiveTexture     = new_tex;
                                    m.EmissiveTexturePath = std::filesystem::relative(abs_path,
                                                            Loom::Project::GetAssetDirectory()).generic_string();
                                    if (modified) modified();
                                }
                            });
                    }
                    if (mrc.EmissiveTexture) {
                        ImGui::SameLine();
                        if (ImGui::Button("X##emissivetex")) {
                            mrc.EmissiveTexture.reset();
                            mrc.EmissiveTexturePath.clear();
                            is_modified = true;
                        }
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("Emissive");
                    ImGui::PopID();
                }

                // ---- Material: normal map (tangent-space; flat fallback when empty)
                {
                    ImTextureID nrm_id = (ImTextureID)(uintptr_t)(mrc.NormalTexture
                        ? mrc.NormalTexture->GetRendererID()
                        : mCheckerboard->GetRendererID());
                    std::string nrm_label = mrc.NormalTexture
                        ? std::filesystem::path(mrc.NormalTexture->GetPath()).filename().string()
                        : "None (Select...)";

                    ImGui::PushID("NormalTexSlot");
                    ImGui::Image(nrm_id, ImVec2(32, 32), ImVec2(0, 1), ImVec2(1, 0),
                                 ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 0.5f));
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Tangent-space normal map.\nRGB encodes the surface normal perturbation;\nblue-dominant = pointing forward = no perturbation.");
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                            std::filesystem::path dropped((const char*)payload->Data);
                            auto ext = dropped.extension();
                            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
                                auto full = Loom::Project::GetAssetFileSystemPath(dropped);
                                auto new_tex = Loom::AssetManager::GetTexture(
                                    full.generic_string(), Loom::kMeshAlbedoTextureSpec);
                                if (new_tex) {
                                    mrc.NormalTexture     = new_tex;
                                    mrc.NormalTexturePath = dropped.generic_string();
                                    is_modified = true;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(nrm_label.c_str(), ImVec2(150, 0))) {
                        auto uuid     = entity.GetComponent<Loom::IDComponent>().ID;
                        auto scene    = mContext;
                        auto modified = mSceneModifiedCallback;
                        FileDialog::Open("BrowseNormalTex", "Choose Normal Map Texture",
                            ".png,.jpg,.jpeg,.bmp,.tga",
                            [uuid, scene, modified](const std::string& abs_path) {
                                Loom::Entity e = scene->GetEntityByUUID(uuid);
                                if (!e || !e.HasComponent<Loom::MeshRendererComponent>()) return;
                                auto new_tex = Loom::AssetManager::GetTexture(abs_path, Loom::kMeshAlbedoTextureSpec);
                                if (new_tex) {
                                    auto& m = e.GetComponent<Loom::MeshRendererComponent>();
                                    m.NormalTexture     = new_tex;
                                    m.NormalTexturePath = std::filesystem::relative(abs_path,
                                                            Loom::Project::GetAssetDirectory()).generic_string();
                                    if (modified) modified();
                                }
                            });
                    }
                    if (mrc.NormalTexture) {
                        ImGui::SameLine();
                        if (ImGui::Button("X##normaltex")) {
                            mrc.NormalTexture.reset();
                            mrc.NormalTexturePath.clear();
                            is_modified = true;
                        }
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("Normal Map");
                    ImGui::PopID();
                }

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

        if (entity.HasComponent<Loom::Rigidbody3DComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::Rigidbody3DComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Rigidbody 3D");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& rb          = entity.GetComponent<Loom::Rigidbody3DComponent>();
                bool  is_modified = false;

                static const char* k_body_type_labels[] = { "Static", "Dynamic", "Kinematic" };
                int type_idx = (int)rb.Type;
                if (ImGui::Combo("Body Type", &type_idx, k_body_type_labels, 3)) {
                    rb.Type = (Loom::Rigidbody3DComponent::BodyType)type_idx;
                    is_modified = true;
                }
                is_modified |= ImGui::Checkbox("Fixed Rotation",  &rb.FixedRotation);
                is_modified |= ImGui::DragFloat("Linear Damping",  &rb.LinearDamping,  0.005f, 0.0f, 1.0f);
                is_modified |= ImGui::DragFloat("Angular Damping", &rb.AngularDamping, 0.005f, 0.0f, 1.0f);

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::Rigidbody3DComponent>("Rigidbody 3D");
        }

        if (entity.HasComponent<Loom::BoxCollider3DComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::BoxCollider3DComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Box Collider 3D");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& bc          = entity.GetComponent<Loom::BoxCollider3DComponent>();
                bool  is_modified = false;
                is_modified |= ImGui::DragFloat3("Offset",       glm::value_ptr(bc.Offset),      0.05f);
                is_modified |= ImGui::DragFloat3("Half Extents", glm::value_ptr(bc.HalfExtents), 0.05f, 0.001f, 1000.0f);
                is_modified |= ImGui::DragFloat ("Density",      &bc.Density,      0.1f, 0.0f, 100000.0f);
                is_modified |= ImGui::DragFloat ("Friction",     &bc.Friction,     0.01f, 0.0f, 1.0f);
                is_modified |= ImGui::DragFloat ("Restitution",  &bc.Restitution,  0.01f, 0.0f, 1.0f);
                is_modified |= ImGui::Checkbox  ("Is Sensor",    &bc.IsSensor);

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::BoxCollider3DComponent>("Box Collider 3D");
        }

        if (entity.HasComponent<Loom::SphereCollider3DComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::SphereCollider3DComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Sphere Collider 3D");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& sc          = entity.GetComponent<Loom::SphereCollider3DComponent>();
                bool  is_modified = false;
                is_modified |= ImGui::DragFloat3("Offset",      glm::value_ptr(sc.Offset), 0.05f);
                is_modified |= ImGui::DragFloat ("Radius",      &sc.Radius,      0.05f, 0.001f, 1000.0f);
                is_modified |= ImGui::DragFloat ("Density",     &sc.Density,     0.1f, 0.0f, 100000.0f);
                is_modified |= ImGui::DragFloat ("Friction",    &sc.Friction,    0.01f, 0.0f, 1.0f);
                is_modified |= ImGui::DragFloat ("Restitution", &sc.Restitution, 0.01f, 0.0f, 1.0f);
                is_modified |= ImGui::Checkbox  ("Is Sensor",   &sc.IsSensor);

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::SphereCollider3DComponent>("Sphere Collider 3D");
        }

        if (entity.HasComponent<Loom::CapsuleCollider3DComponent>()) {
            bool remove_component = false;
            bool opened = ImGui::TreeNodeEx((void*)typeid(Loom::CapsuleCollider3DComponent).hash_code(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap, "Capsule Collider 3D");

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove Component")) remove_component = true;
                ImGui::EndPopup();
            }

            if (opened) {
                auto& cc          = entity.GetComponent<Loom::CapsuleCollider3DComponent>();
                bool  is_modified = false;
                is_modified |= ImGui::DragFloat3("Offset",      glm::value_ptr(cc.Offset), 0.05f);
                is_modified |= ImGui::DragFloat ("Radius",      &cc.Radius,      0.05f, 0.001f, 1000.0f);
                is_modified |= ImGui::DragFloat ("Half Height", &cc.HalfHeight,  0.05f, 0.001f, 1000.0f);
                is_modified |= ImGui::DragFloat ("Density",     &cc.Density,     0.1f,  0.0f,   100000.0f);
                is_modified |= ImGui::DragFloat ("Friction",    &cc.Friction,    0.01f, 0.0f,   1.0f);
                is_modified |= ImGui::DragFloat ("Restitution", &cc.Restitution, 0.01f, 0.0f,   1.0f);
                is_modified |= ImGui::Checkbox  ("Is Sensor",   &cc.IsSensor);

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) push_remove.template operator()<Loom::CapsuleCollider3DComponent>("Capsule Collider 3D");
        }

        ImGui::PopID();
    }

} // namespace Weaver
