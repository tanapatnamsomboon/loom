#include "scene_hierarchy_panel.h"
#include <loom/asset/asset_manager.h>
#include <loom/scene/components.h>
#include <loom/scene/script_registry.h>
#include <nfd.hpp>
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <loom/project/project.h>
#include <loom/scripting/scripting_engine.h>
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
                Loom::Entity child = mContext->CreateEntity("Child Entity");
                mContext->SetParent(child, entity);
                if (mSceneModifiedCallback) mSceneModifiedCallback();
            }
            if (has_parent) {
                if (ImGui::MenuItem("Detach from Parent")) {
                    mContext->RemoveParent(entity);
                    if (mSceneModifiedCallback) mSceneModifiedCallback();
                }
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
            // Walk selection's ancestor chain to detect if it lives under the deleted entity
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
            mContext->DestroyEntity(entity);
            if (clear_selection) mSelectionContext = {};
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
            if (!mSelectionContext.HasComponent<Loom::Rigidbody2DComponent>()) {
                if (ImGui::MenuItem("Rigidbody 2D")) {
                    mSelectionContext.AddComponent<Loom::Rigidbody2DComponent>();
                    if (mSceneModifiedCallback) mSceneModifiedCallback();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!mSelectionContext.HasComponent<Loom::BoxCollider2DComponent>()) {
                if (ImGui::MenuItem("Box Collider 2D")) {
                    mSelectionContext.AddComponent<Loom::BoxCollider2DComponent>();
                    if (mSceneModifiedCallback) mSceneModifiedCallback();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!mSelectionContext.HasComponent<Loom::CircleCollider2DComponent>()) {
                if (ImGui::MenuItem("Circle Collider 2D")) {
                    mSelectionContext.AddComponent<Loom::CircleCollider2DComponent>();
                    if (mSceneModifiedCallback) mSceneModifiedCallback();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!mSelectionContext.HasComponent<Loom::LuaScriptComponent>()) {
                if (ImGui::MenuItem("Lua Script")) {
                    mSelectionContext.AddComponent<Loom::LuaScriptComponent>();
                    if (mSceneModifiedCallback) mSceneModifiedCallback();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!mSelectionContext.HasComponent<Loom::AnimationComponent>()) {
                if (ImGui::MenuItem("Sprite Animator")) {
                    mSelectionContext.AddComponent<Loom::AnimationComponent>();
                    if (mSceneModifiedCallback) mSceneModifiedCallback();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }

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
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::filesystem::path dropped((const char*)payload->Data);
                        auto ext = dropped.extension();
                        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
                            auto full = Loom::Project::GetAssetFileSystemPath(dropped);
                            auto new_texture = Loom::AssetManager::GetTexture(full.generic_string());
                            if (new_texture) { texture = new_texture; is_modified = true; }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
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

            if (remove_component) {
                entity.RemoveComponent<Loom::Rigidbody2DComponent>();
                if (mSceneModifiedCallback) mSceneModifiedCallback();
            }
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

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) {
                entity.RemoveComponent<Loom::BoxCollider2DComponent>();
                if (mSceneModifiedCallback) mSceneModifiedCallback();
            }
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

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) {
                entity.RemoveComponent<Loom::CircleCollider2DComponent>();
                if (mSceneModifiedCallback) mSceneModifiedCallback();
            }
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
                    constexpr nfdfilteritem_t filters[] = {
                        { "Lua Scripts", "lua" },
                        { "All Files",   "*"   },
                    };
                    NFD::Guard      guard;
                    NFD::UniquePath out_path;
                    if (NFD::OpenDialog(out_path, filters, 2) == NFD_OKAY) {
                        std::filesystem::path picked(out_path.get());
                        std::filesystem::path asset_dir = Loom::Project::GetAssetDirectory();
                        std::error_code       ec;
                        auto rel = std::filesystem::relative(picked, asset_dir, ec);
                        ls.ScriptPath = (!ec && !rel.empty() && rel.string().find("..") == std::string::npos)
                            ? rel.generic_string() : picked.generic_string();
                        is_modified = true;
                    }
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

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) {
                entity.RemoveComponent<Loom::LuaScriptComponent>();
                if (mSceneModifiedCallback) mSceneModifiedCallback();
            }
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

                if (is_modified && mSceneModifiedCallback) mSceneModifiedCallback();
                ImGui::TreePop();
            }

            if (remove_component) {
                entity.RemoveComponent<Loom::AnimationComponent>();
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
