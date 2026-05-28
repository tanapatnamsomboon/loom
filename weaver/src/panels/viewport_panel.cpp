#include "viewport_panel.h"
#include "editor/commands.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <loom/asset/asset_manager.h>
#include <loom/core/application.h>
#include <loom/core/input.h>
#include <loom/math/math.h>
#include <loom/project/project.h>
#include <loom/renderer/render_command.h>
#include <loom/renderer/renderer_3d.h>
#include <loom/scene/components.h>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace Weaver {

    // ────────────────────────────────────────────────────────────────────────
    // Construction
    // ────────────────────────────────────────────────────────────────────────
    ViewportPanel::ViewportPanel(EditorContext& ctx)
        : mContext(ctx) {}

    void ViewportPanel::Init() {
        std::string grid_path = Loom::Project::GetEngineAssetFileSystemPath("shaders/grid").generic_string();

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
    }

    void ViewportPanel::BeginFrame() {
        HandleViewportResize();
        mPipeline.BeginScene();
    }

    void ViewportPanel::HandleViewportResize() {
        ComputeGameViewRect();

        // DPI: ImGui reports sizes in logical pixels. On a 125% / 150% scaled
        // Windows display the physical framebuffer is bigger, so rendering at
        // logical size then letting ImGui's backend stretch the result to
        // physical pixels produces a bilinear softening that FXAA compounds.
        // Size the offscreen FBOs in physical pixels; ImGui::Image still takes
        // logical pixels and the backend's internal scaling lines us up 1:1.
        const ImVec2 dpi  = ImGui::GetIO().DisplayFramebufferScale;
        const float  fb_w = mGameViewSize.x * dpi.x;
        const float  fb_h = mGameViewSize.y * dpi.y;

        mContext.ActiveScene->OnViewportResize((uint32_t)fb_w, (uint32_t)fb_h);

        if (fb_w > 0.0f && fb_h > 0.0f &&
            (mPipeline.GetWidth() != (uint32_t)fb_w || mPipeline.GetHeight() != (uint32_t)fb_h)) {
            mPipeline.Resize((uint32_t)fb_w, (uint32_t)fb_h);
            // Editor camera aspect uses the ratio only; passing physical sizes
            // is fine and keeps any internal pixel-based logic consistent with
            // the pipeline above.
            mContext.EditorCamera.SetViewportSize(fb_w, fb_h);
        }
    }

    void ViewportPanel::ComputeGameViewRect() {
        const float panel_w = mContext.ViewportSize.x;
        const float panel_h = mContext.ViewportSize.y;

        mGameViewSize   = { panel_w, panel_h };
        mGameViewOffset = { 0.0f, 0.0f };

        if (panel_w <= 0.0f || panel_h <= 0.0f) return;
        if (mContext.SceneState != SceneState::Play) return;
        if (!mContext.ActiveScene) return;

        float target_aspect = 0.0f;
        auto view = mContext.ActiveScene->GetAllEntitiesWith<Loom::CameraComponent>();
        for (auto entity : view) {
            const auto& cc = view.get<Loom::CameraComponent>(entity);
            if (cc.Primary && cc.FixedAspectRatio && cc.AspectRatio > 0.0f) {
                target_aspect = cc.AspectRatio;
                break;
            }
        }
        if (target_aspect <= 0.0f) return;

        const float panel_aspect = panel_w / panel_h;
        if (target_aspect > panel_aspect) {
            mGameViewSize = { panel_w, panel_w / target_aspect };
        } else {
            mGameViewSize = { panel_h * target_aspect, panel_h };
        }
        mGameViewOffset = { (panel_w - mGameViewSize.x) * 0.5f,
                            (panel_h - mGameViewSize.y) * 0.5f };
    }

    void ViewportPanel::RenderScene(Loom::Timestep ts) {
        if (!Loom::Project::GetActive())
            return;

        if (mContext.SceneState == SceneState::Edit) {
            if (mContext.ViewportHovered)
                mContext.EditorCamera.OnUpdate(ts);
            else
                mContext.EditorCamera.ResetMousePosition();

            auto scene_skybox     = mContext.ActiveScene->GetSkyboxCubemap();
            auto scene_irradiance = mContext.ActiveScene->GetIrradianceCubemap();
            auto effective_skybox     = scene_skybox     ? scene_skybox
                                                         : mContext.FallbackEnvironment.Skybox;
            auto effective_irradiance = scene_irradiance ? scene_irradiance
                                                         : mContext.FallbackEnvironment.Irradiance;
            auto debug_cubemap = (mContext.ActiveScene->GetSkyboxSource() == Loom::Scene::SkyboxSource::Irradiance)
                                 ? effective_irradiance
                                 : effective_skybox;
            Loom::Renderer3D::DrawSkybox(mContext.EditorCamera.GetViewMatrix(),
                                          mContext.EditorCamera.GetProjectionMatrix(),
                                          debug_cubemap);
        }

        switch (mContext.SceneState) {
            case SceneState::Edit:
                mContext.ActiveScene->OnUpdateEditor(ts, mContext.EditorCamera,
                                                     mContext.HierarchyPanel->GetSelectedEntity(),
                                                     mContext.FallbackEnvironment.Irradiance,
                                                     mContext.FallbackEnvironment.Prefilter);
                break;
            case SceneState::Play:
                mContext.ActiveScene->OnUpdateRuntime(ts);
                break;
        }

        if (mContext.SceneState == SceneState::Edit) {
            // Grid must draw AFTER meshes so its alpha blend uses the current
            // framebuffer color (sky or mesh) instead of haloing each line.
            const auto& gs       = mContext.Grid;
            glm::vec3   cam_pos  = mContext.EditorCamera.GetPosition();
            glm::mat4   grid_transform = glm::translate(glm::mat4(1.0f), { cam_pos.x, 0.0f, cam_pos.z })
                                       * glm::scale(glm::mat4(1.0f), { 150.0f, 1.0f, 150.0f });
            mGridShader->Bind();
            mGridShader->UploadUniformMat4("uViewProjection",  mContext.EditorCamera.GetViewProjectionMatrix());
            mGridShader->UploadUniformMat4("uTransform",       grid_transform);
            mGridShader->UploadUniformFloat3("uCameraPosition", cam_pos);
            mGridShader->UploadUniformFloat("uMinorScale",     gs.MinorScale);
            mGridShader->UploadUniformFloat("uMajorScale",     gs.MajorScale);
            mGridShader->UploadUniformFloat("uLineThickness",  gs.LineThickness);
            mGridShader->UploadUniformFloat("uFadeStart",      gs.FadeStart);
            mGridShader->UploadUniformFloat("uFadeEnd",        gs.FadeEnd);
            mGridShader->UploadUniformFloat4("uMinorColor",    gs.MinorColor);
            mGridShader->UploadUniformFloat4("uMajorColor",    gs.MajorColor);
            Loom::RenderCommand::DrawIndexed(mGridVAO.get(), 6);
        }
    }

    void ViewportPanel::UpdateHoveredEntity() {
        auto [mx, my] = ImGui::GetMousePos();
        mx -= mContext.ViewportBounds[0].x + mGameViewOffset.x;
        my -= mContext.ViewportBounds[0].y + mGameViewOffset.y;
        my = mGameViewSize.y - my;

        // ImGui mouse pos is in logical pixels; the picking FBO is in physical
        // pixels (see HandleViewportResize). Scale mouse coords before sampling.
        const ImVec2 dpi   = ImGui::GetIO().DisplayFramebufferScale;
        const float  fb_w  = mGameViewSize.x * dpi.x;
        const float  fb_h  = mGameViewSize.y * dpi.y;
        const int    mouse_x = (int)(mx * dpi.x);
        const int    mouse_y = (int)(my * dpi.y);

        if (mouse_x >= 0 && mouse_y >= 0 && mouse_x < (int)fb_w && mouse_y < (int)fb_h) {
            int pixel = mPipeline.ReadPickingPixel(mouse_x, mouse_y);
            mContext.HoveredEntity = (pixel == -1)
                ? Loom::Entity()
                : Loom::Entity((entt::entity)pixel, mContext.ActiveScene.get());
        } else {
            mContext.HoveredEntity = Loom::Entity();
        }
    }

    void ViewportPanel::EndFrame() {
        mPipeline.EndScene();
    }

    void ViewportPanel::OnImGuiRender() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });

        std::string title = "Viewport###Viewport";
        if (Loom::Project::GetActive()) {
            std::string filename = mContext.CurrentScenePath.empty()
                ? "Untitled Scene"
                : std::filesystem::path(mContext.CurrentScenePath).filename().string();
            title = filename + (mContext.IsDirty() ? "*" : "") + " (Viewport)###Viewport";
        }

        ImGui::Begin(title.c_str());

        mContext.ViewportFocused = ImGui::IsWindowFocused();
        mContext.ViewportHovered = ImGui::IsWindowHovered();

        // RMB force-focus: ImGui only LMB-focuses windows; without this, RMB-orbiting
        // would route WASD into whatever textbox last held focus.
        if (mContext.ViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::SetWindowFocus();
            mContext.ViewportFocused = true;
        }

        Loom::Application::Get().GetImGuiLayer()->BlockEvents(!mContext.ViewportHovered);

        UpdateViewportBounds();
        UpdateViewportSize();

        uint32_t tex_id = mPipeline.GetFinalColorTextureID();

        ImVec2 image_pos = ImVec2{ mContext.ViewportBounds[0].x + mGameViewOffset.x,
                                   mContext.ViewportBounds[0].y + mGameViewOffset.y };
        ImGui::SetCursorScreenPos(image_pos);
        ImGui::Image((void*)(intptr_t)tex_id,
                     ImVec2{ mGameViewSize.x, mGameViewSize.y },
                     ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

        if (mGameViewOffset.x > 0.0f || mGameViewOffset.y > 0.0f) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImU32 bar_col = IM_COL32(0, 0, 0, 255);
            ImVec2 vp_tl = ImVec2{ mContext.ViewportBounds[0].x, mContext.ViewportBounds[0].y };
            ImVec2 vp_br = ImVec2{ mContext.ViewportBounds[1].x, mContext.ViewportBounds[1].y };
            if (mGameViewOffset.x > 0.0f) {
                dl->AddRectFilled(vp_tl, ImVec2{ image_pos.x, vp_br.y }, bar_col);
                dl->AddRectFilled(ImVec2{ image_pos.x + mGameViewSize.x, vp_tl.y }, vp_br, bar_col);
            }
            if (mGameViewOffset.y > 0.0f) {
                dl->AddRectFilled(vp_tl, ImVec2{ vp_br.x, image_pos.y }, bar_col);
                dl->AddRectFilled(ImVec2{ vp_tl.x, image_pos.y + mGameViewSize.y }, vp_br, bar_col);
            }
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                std::filesystem::path dropped((const char*)payload->Data);
                if (dropped.extension() == ".loom" && mSceneOpenCallback) {
                    mSceneOpenCallback(dropped);
                } else if (dropped.extension() == ".lprefab" && mPrefabInstantiateCallback) {
                    mPrefabInstantiateCallback(dropped);
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (mContext.Tool == ToolMode::TilePaint) {
            RenderTilePaint();
        } else {
            RenderGizmo();
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void ViewportPanel::UpdateViewportBounds() {
        auto min_region = ImGui::GetWindowContentRegionMin();
        auto max_region = ImGui::GetWindowContentRegionMax();
        auto offset     = ImGui::GetWindowPos();

        mContext.ViewportBounds[0] = { min_region.x + offset.x, min_region.y + offset.y };
        mContext.ViewportBounds[1] = { max_region.x + offset.x, max_region.y + offset.y };
    }

    void ViewportPanel::UpdateViewportSize() {
        ImVec2 content    = ImGui::GetContentRegionAvail();
        mContext.ViewportSize = { content.x, content.y };
    }

    // ────────────────────────────────────────────────────────────────────────
    // Gizmo (ImGuizmo)
    // ────────────────────────────────────────────────────────────────────────
    bool ViewportPanel::IsGizmoBusy() const {
        return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
    }

    void ViewportPanel::RenderGizmo() {
        if (mContext.SceneState != SceneState::Edit) return;
        if (mContext.GizmoOp == GizmoOperation::None) return;
        Loom::Entity selected = mContext.HierarchyPanel ? mContext.HierarchyPanel->GetSelectedEntity() : Loom::Entity();
        if (!selected || !selected.HasComponent<Loom::TransformComponent>()) return;
        if (mContext.ViewportSize.x <= 0.0f || mContext.ViewportSize.y <= 0.0f) return;

        // SetAlternativeWindow is required: ImGuizmo's BeginFrame creates an internal
        // NoInputs window for hit-testing; without this hint, IsHoveringWindow fails
        // against our viewport and gizmo hover silently dies.
        ImGuizmo::PushID(0);
        ImGuizmo::Enable(true);
        ImGuizmo::AllowAxisFlip(false);
        ImGuizmo::SetGizmoSizeClipSpace(0.1f);
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetAlternativeWindow(ImGui::GetCurrentWindow());
        ImGuizmo::SetRect(mContext.ViewportBounds[0].x, mContext.ViewportBounds[0].y,
                          mContext.ViewportSize.x,      mContext.ViewportSize.y);

        ImGuizmo::OPERATION op;
        switch (mContext.GizmoOp) {
            case GizmoOperation::Translate: op = ImGuizmo::TRANSLATE; break;
            case GizmoOperation::Rotate:    op = ImGuizmo::ROTATE;    break;
            case GizmoOperation::Scale:     op = ImGuizmo::SCALE;     break;
            default:                        op = ImGuizmo::TRANSLATE; break;
        }
        // Scale is meaningless in WORLD mode (Maya/Unreal convention); force LOCAL.
        ImGuizmo::MODE mode = (mContext.GizmoMode == GizmoSpace::World && mContext.GizmoOp != GizmoOperation::Scale)
                            ? ImGuizmo::WORLD
                            : ImGuizmo::LOCAL;

        auto& tc            = selected.GetComponent<Loom::TransformComponent>();
        Loom::Entity parent = selected.GetParent();
        glm::mat4 parent_world     = parent ? mContext.ActiveScene->GetWorldTransform(parent) : glm::mat4(1.0f);
        glm::mat4 parent_world_inv = glm::inverse(parent_world);
        glm::mat4 entity_world     = parent_world * tc.GetTransform();

        glm::mat4 view = mContext.EditorCamera.GetViewMatrix();
        glm::mat4 proj = mContext.EditorCamera.GetProjectionMatrix();

        // Hold Ctrl to snap (Unity / Unreal convention). Polled live each
        // frame so the user can toggle the modifier mid-drag.
        // ImGuizmo reads 3 floats for TRANSLATE (per-axis), 1 for ROTATE/SCALE.
        float snap_values[3] = { 0.0f, 0.0f, 0.0f };
        const float* snap_ptr = nullptr;
        if (ImGui::GetIO().KeyCtrl) {
            switch (mContext.GizmoOp) {
                case GizmoOperation::Translate:
                    snap_values[0] = snap_values[1] = snap_values[2] = mContext.TranslateSnap;
                    snap_ptr = snap_values;
                    break;
                case GizmoOperation::Rotate:
                    snap_values[0] = mContext.RotateSnap;
                    snap_ptr = snap_values;
                    break;
                case GizmoOperation::Scale:
                    snap_values[0] = mContext.ScaleSnap;
                    snap_ptr = snap_values;
                    break;
                default: break;
            }
        }

        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                             op, mode,
                             glm::value_ptr(entity_world),
                             nullptr,
                             snap_ptr);

        // Drag-start: snapshot for a single batched TransformEditCommand on drag-end.
        bool using_now = ImGuizmo::IsUsing();
        if (using_now && !mGizmoWasUsing) {
            mDragEntityUUID = (uint64_t)selected.GetComponent<Loom::IDComponent>().ID;
            mDragStartLocal = tc;
        }

        if (using_now) {
            glm::mat4 new_local = parent_world_inv * entity_world;
            glm::vec3 t, r, s;
            if (Loom::Math::DecomposeTransform(new_local, t, r, s)) {
                tc.Translation = t;
                tc.Rotation    = r;
                tc.Scale       = s;
            }
        }

        if (!using_now && mGizmoWasUsing && mDragEntityUUID != 0) {
            // UUID lookup: selection may have changed between drag start and end.
            Loom::Entity e = mContext.ActiveScene ? mContext.ActiveScene->GetEntityByUUID(Loom::UUID{ mDragEntityUUID })
                                                  : Loom::Entity();
            if (e && e.HasComponent<Loom::TransformComponent>()) {
                auto& current = e.GetComponent<Loom::TransformComponent>();
                const float kEps = 1e-5f;
                auto changed = [&](const glm::vec3& a, const glm::vec3& b) {
                    return std::abs(a.x - b.x) > kEps || std::abs(a.y - b.y) > kEps || std::abs(a.z - b.z) > kEps;
                };
                if (changed(current.Translation, mDragStartLocal.Translation) ||
                    changed(current.Rotation,    mDragStartLocal.Rotation)    ||
                    changed(current.Scale,       mDragStartLocal.Scale)) {
                    mContext.History.Push(std::make_unique<TransformEditCommand>(
                        mContext.ActiveScene,
                        Loom::UUID{ mDragEntityUUID },
                        mDragStartLocal,
                        current));
                }
            }
            mDragEntityUUID = 0;
        }

        ImGuizmo::PopID();
        mGizmoWasUsing = using_now;
    }

    // ────────────────────────────────────────────────────────────────────────
    // Tile paint
    // ────────────────────────────────────────────────────────────────────────
    bool ViewportPanel::IsTilePaintActive() const {
        if (mContext.Tool != ToolMode::TilePaint) return false;
        if (mContext.SceneState != SceneState::Edit) return false;
        Loom::Entity selected = mContext.HierarchyPanel ? mContext.HierarchyPanel->GetSelectedEntity()
                                                        : Loom::Entity();
        if (!selected || !selected.HasComponent<Loom::TilemapComponent>()) return false;
        return true;
    }

    void ViewportPanel::RenderTilePaint() {
        if (!IsTilePaintActive()) return;
        if (!mContext.ActiveScene) return;

        Loom::Entity selected = mContext.HierarchyPanel->GetSelectedEntity();
        auto& tc = selected.GetComponent<Loom::TilemapComponent>();

        // Defensive: scripts/serializer may have drifted Tiles size from map dims.
        int expected = tc.Columns * tc.Rows;
        if ((int)tc.Tiles.size() != expected) tc.Tiles.assign(expected, -1);

        glm::vec2 vp_size = mContext.ViewportSize;
        if (vp_size.x <= 0.0f || vp_size.y <= 0.0f) return;

        glm::mat4 view = mContext.EditorCamera.GetViewMatrix();
        glm::mat4 proj = mContext.EditorCamera.GetProjectionMatrix();
        glm::mat4 vp   = proj * view;

        glm::mat4 world      = mContext.ActiveScene->GetWorldTransform(selected);
        glm::vec3 plane_pt   = glm::vec3(world[3]);
        glm::vec3 plane_n_w  = glm::vec3(world[2]);
        float plane_n_len    = glm::length(plane_n_w);
        if (plane_n_len < 1e-6f) return;
        plane_n_w /= plane_n_len;

        float hw = tc.Columns * tc.TileWidth  * 0.5f;
        float hh = tc.Rows    * tc.TileHeight * 0.5f;

        auto local_to_abs = [&](glm::vec3 local) -> std::optional<ImVec2> {
            glm::vec4 ws = world * glm::vec4(local, 1.0f);
            auto vp_px = Loom::Math::WorldToScreen(glm::vec3(ws), vp_size, vp);
            if (!vp_px) return std::nullopt;
            return ImVec2{ vp_px->x + mContext.ViewportBounds[0].x,
                           vp_px->y + mContext.ViewportBounds[0].y };
        };

        // Window draw list (not foreground) so the overlay respects the viewport clip rect.
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Border + interior lines walk rows/cols separately — O(R+C), not O(R*C).
        ImU32 grid_col   = IM_COL32(255, 255, 255,  90);
        ImU32 border_col = IM_COL32(255, 200,  80, 220);

        auto tl = local_to_abs({ -hw,  hh, 0.0f });
        auto tr = local_to_abs({  hw,  hh, 0.0f });
        auto br = local_to_abs({  hw, -hh, 0.0f });
        auto bl = local_to_abs({ -hw, -hh, 0.0f });
        if (tl && tr && br && bl) {
            dl->AddLine(*tl, *tr, border_col, 1.5f);
            dl->AddLine(*tr, *br, border_col, 1.5f);
            dl->AddLine(*br, *bl, border_col, 1.5f);
            dl->AddLine(*bl, *tl, border_col, 1.5f);
        }
        for (int r = 1; r < tc.Rows; ++r) {
            float y = hh - r * tc.TileHeight;
            auto a = local_to_abs({ -hw, y, 0.0f });
            auto b = local_to_abs({  hw, y, 0.0f });
            if (a && b) dl->AddLine(*a, *b, grid_col, 1.0f);
        }
        for (int c = 1; c < tc.Columns; ++c) {
            float x = -hw + c * tc.TileWidth;
            auto a = local_to_abs({ x, -hh, 0.0f });
            auto b = local_to_abs({ x,  hh, 0.0f });
            if (a && b) dl->AddLine(*a, *b, grid_col, 1.0f);
        }

        if (!tc.Solid.empty()) {
            ImU32 solid_fill = IM_COL32(220, 60, 60, 70);
            for (int rr = 0; rr < tc.Rows; ++rr) {
                for (int cc = 0; cc < tc.Columns; ++cc) {
                    int idx = tc.Tiles[rr * tc.Columns + cc];
                    if (idx < 0 || idx >= (int)tc.Solid.size() || !tc.Solid[idx]) continue;
                    float x0 = -hw + cc * tc.TileWidth;
                    float x1 = x0 + tc.TileWidth;
                    float y1 = hh - rr * tc.TileHeight;
                    float y0 = y1 - tc.TileHeight;
                    auto s_tl = local_to_abs({ x0, y1, 0.0f });
                    auto s_tr = local_to_abs({ x1, y1, 0.0f });
                    auto s_br = local_to_abs({ x1, y0, 0.0f });
                    auto s_bl = local_to_abs({ x0, y0, 0.0f });
                    if (s_tl && s_tr && s_br && s_bl) {
                        ImVec2 quad[4] = { *s_tl, *s_tr, *s_br, *s_bl };
                        dl->AddConvexPolyFilled(quad, 4, solid_fill);
                    }
                }
            }
        }

        if (!mContext.ViewportHovered) return;
        if (ImGui::GetIO().WantTextInput) return;

        ImVec2 mouse_abs = ImGui::GetMousePos();
        glm::vec2 mouse_vp = { mouse_abs.x - mContext.ViewportBounds[0].x,
                               mouse_abs.y - mContext.ViewportBounds[0].y };

        Loom::Math::Ray ray = Loom::Math::ScreenToRay(mouse_vp, vp_size, view, proj);
        auto hit_world = Loom::Math::RayPlaneIntersect(ray, plane_pt, plane_n_w);
        if (!hit_world) return;

        glm::vec3 hit_local = glm::vec3(glm::inverse(world) * glm::vec4(*hit_world, 1.0f));
        int col = (int)std::floor((hit_local.x + hw) / tc.TileWidth);
        int row = (int)std::floor((hh - hit_local.y) / tc.TileHeight);
        if (col < 0 || col >= tc.Columns || row < 0 || row >= tc.Rows) return;

        float cx0 = -hw + col * tc.TileWidth;
        float cx1 = cx0 + tc.TileWidth;
        float cy1 = hh - row * tc.TileHeight;
        float cy0 = cy1 - tc.TileHeight;
        auto p_tl = local_to_abs({ cx0, cy1, 0.0f });
        auto p_tr = local_to_abs({ cx1, cy1, 0.0f });
        auto p_br = local_to_abs({ cx1, cy0, 0.0f });
        auto p_bl = local_to_abs({ cx0, cy0, 0.0f });
        if (p_tl && p_tr && p_br && p_bl) {
            ImU32 fill = (mContext.SelectedTileIndex < 0)
                       ? IM_COL32(220,  60,  60, 110)
                       : IM_COL32( 80, 200, 120, 110);
            ImVec2 quad[4] = { *p_tl, *p_tr, *p_br, *p_bl };
            dl->AddConvexPolyFilled(quad, 4, fill);
            dl->AddPolyline(quad, 4, IM_COL32(255, 255, 255, 240), ImDrawFlags_Closed, 2.0f);
        }

        // Paint is idempotent per-cell: only mutate if the tile would change.
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            int& tile = tc.Tiles[row * tc.Columns + col];
            if (tile != mContext.SelectedTileIndex) {
                tile = mContext.SelectedTileIndex;
                mContext.SceneDirty = true;
            }
        }
    }

} // namespace Weaver
