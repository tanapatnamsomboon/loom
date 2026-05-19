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
#include <loom/scene/components.h>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace Weaver {

    // ────────────────────────────────────────────────────────────────────────
    // Construction
    // ────────────────────────────────────────────────────────────────────────
    ViewportPanel::ViewportPanel(EditorContext& ctx)
        : mContext(ctx) {
        Loom::FramebufferSpecification spec;
        spec.Attachments = {
            Loom::FramebufferTextureFormat::RGBA8,
            Loom::FramebufferTextureFormat::RED_INTEGER,
            Loom::FramebufferTextureFormat::DEPTH24STENCIL8
        };
        spec.Width   = 1280;
        spec.Height  = 720;
        mFramebuffer = Loom::Framebuffer::Create(spec);
    }

    void ViewportPanel::Init() {
        std::string skybox_path = Loom::Project::GetEngineAssetFileSystemPath("shaders/skybox").generic_string();
        std::string grid_path   = Loom::Project::GetEngineAssetFileSystemPath("shaders/grid").generic_string();

        float skybox_vertices[] = {
            -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f
        };
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

        mFramebuffer->Bind();
        Loom::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        Loom::RenderCommand::Clear();
        mFramebuffer->ClearAttachment(1, -1);
    }

    void ViewportPanel::HandleViewportResize() {
        mContext.ActiveScene->OnViewportResize((uint32_t)mContext.ViewportSize.x, (uint32_t)mContext.ViewportSize.y);

        Loom::FramebufferSpecification spec = mFramebuffer->GetSpecification();
        if (mContext.ViewportSize.x > 0.0f && mContext.ViewportSize.y > 0.0f &&
            (spec.Width != mContext.ViewportSize.x || spec.Height != mContext.ViewportSize.y)) {
            mFramebuffer->Resize((uint32_t)mContext.ViewportSize.x, (uint32_t)mContext.ViewportSize.y);
            mContext.EditorCamera.SetViewportSize(mContext.ViewportSize.x, mContext.ViewportSize.y);
        }
    }

    void ViewportPanel::RenderScene(Loom::Timestep ts) {
        if (!Loom::Project::GetActive())
            return;

        if (mContext.SceneState == SceneState::Edit) {
            if (mContext.ViewportHovered)
                mContext.EditorCamera.OnUpdate(ts);
            else
                mContext.EditorCamera.ResetMousePosition();

            // Skybox — HDR cubemap if the scene has one assigned, otherwise
            // skip and let the framebuffer clear color show through (dark grey).
            // GetActiveSkyboxCubemap returns env by default; switches to the
            // irradiance map when the IBL debug toggle is set in the toolbar.
            auto skybox_cube = mContext.ActiveScene->GetActiveSkyboxCubemap();
            if (skybox_cube) {
                glm::mat4 view    = mContext.EditorCamera.GetViewMatrix();
                view[3]           = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                glm::mat4 skyboxVP = mContext.EditorCamera.GetProjectionMatrix() * view;
                mSkyboxShader->Bind();
                mSkyboxShader->UploadUniformMat4("uViewProjection", skyboxVP);
                mSkyboxShader->UploadUniformInt ("uSkybox", 0);
                skybox_cube->Bind(0);
                Loom::RenderCommand::DrawIndexed(mSkyboxVAO.get(), 36);
            }

            // Grid
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

        switch (mContext.SceneState) {
            case SceneState::Edit:
                mContext.ActiveScene->OnUpdateEditor(ts, mContext.EditorCamera, mContext.HierarchyPanel->GetSelectedEntity());
                break;
            case SceneState::Play:
                mContext.ActiveScene->OnUpdateRuntime(ts);
                break;
        }
    }

    void ViewportPanel::UpdateHoveredEntity() {
        auto [mx, my] = ImGui::GetMousePos();
        mx -= mContext.ViewportBounds[0].x;
        my -= mContext.ViewportBounds[0].y;

        glm::vec2 size = mContext.ViewportBounds[1] - mContext.ViewportBounds[0];
        my = size.y - my;

        int mouse_x = (int)mx;
        int mouse_y = (int)my;

        if (mouse_x >= 0 && mouse_y >= 0 && mouse_x < (int)size.x && mouse_y < (int)size.y) {
            int pixel = mFramebuffer->ReadPixel(1, mouse_x, mouse_y);
            mContext.HoveredEntity = (pixel == -1)
                ? Loom::Entity()
                : Loom::Entity((entt::entity)pixel, mContext.ActiveScene.get());
        } else {
            mContext.HoveredEntity = Loom::Entity();
        }
    }

    void ViewportPanel::EndFrame() {
        mFramebuffer->Unbind();
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

        // RMB-press inside the viewport force-focuses the window. ImGui defaults
        // to LMB-only focus switching, so RMB-orbiting the camera while an
        // Inspector text field had focus would route WASD into the textbox.
        if (mContext.ViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::SetWindowFocus();
            mContext.ViewportFocused = true;
        }

        Loom::Application::Get().GetImGuiLayer()->BlockEvents(!mContext.ViewportHovered);

        UpdateViewportBounds();
        UpdateViewportSize();

        uint32_t tex_id = mFramebuffer->GetColorAttachmentRendererID(0);
        ImGui::Image((void*)(intptr_t)tex_id, ImVec2{ mContext.ViewportSize.x, mContext.ViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

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

        // Tile paint and the gizmo are mutually exclusive — paint mode hides the gizmo.
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
        // Gating: only in Edit mode, only when something with a Transform is
        // selected, and only when the user has chosen a gizmo op.
        if (mContext.SceneState != SceneState::Edit) return;
        if (mContext.GizmoOp == GizmoOperation::None) return;
        Loom::Entity selected = mContext.HierarchyPanel ? mContext.HierarchyPanel->GetSelectedEntity() : Loom::Entity();
        if (!selected || !selected.HasComponent<Loom::TransformComponent>()) return;
        if (mContext.ViewportSize.x <= 0.0f || mContext.ViewportSize.y <= 0.0f) return;

        // Per-frame ImGuizmo setup. SetAlternativeWindow is the critical fix:
        // ImGuizmo::BeginFrame creates an internal NoInputs "gizmo" window and
        // routes its IsHoveringWindow check against THAT window's name. Since
        // a NoInputs window can never equal HoveredWindow, mbMouseOver stays
        // false → GetMoveType returns MT_NONE → hover silently dies. Telling
        // ImGuizmo that the viewport is the alternative interactive window
        // makes IsHoveringWindow succeed when HoveredWindow == viewport.
        ImGuizmo::PushID(0);
        ImGuizmo::Enable(true);
        ImGuizmo::AllowAxisFlip(false);
        ImGuizmo::SetGizmoSizeClipSpace(0.1f);
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetAlternativeWindow(ImGui::GetCurrentWindow());
        ImGuizmo::SetRect(mContext.ViewportBounds[0].x, mContext.ViewportBounds[0].y,
                          mContext.ViewportSize.x,      mContext.ViewportSize.y);

        // ── Map editor enums to ImGuizmo enums ──
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

        // ── Build matrices for ImGuizmo ──
        auto& tc            = selected.GetComponent<Loom::TransformComponent>();
        Loom::Entity parent = selected.GetParent();
        glm::mat4 parent_world    = parent ? mContext.ActiveScene->GetWorldTransform(parent) : glm::mat4(1.0f);
        glm::mat4 parent_world_inv = glm::inverse(parent_world);
        glm::mat4 entity_world    = parent_world * tc.GetTransform();

        glm::mat4 view = mContext.EditorCamera.GetViewMatrix();
        glm::mat4 proj = mContext.EditorCamera.GetProjectionMatrix();

        // ImGuizmo writes the manipulated world matrix in place.
        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                             op, mode,
                             glm::value_ptr(entity_world),
                             nullptr,  // delta matrix (unused)
                             nullptr); // snap (TODO: wire into EditorContext later)


        // ── Drag-start: snapshot the local transform so we can push a single
        // TransformEditCommand on drag-end (batches the whole drag as one
        // undoable step instead of one per frame).
        bool using_now = ImGuizmo::IsUsing();
        if (using_now && !mGizmoWasUsing) {
            mDragEntityUUID = (uint64_t)selected.GetComponent<Loom::IDComponent>().ID;
            mDragStartLocal = tc;
        }

        // ── While dragging, decompose ImGuizmo's new world matrix back into a
        // local transform on the entity. inverse(parent_world) strips the
        // parent's contribution so parent-aware drags work correctly.
        if (using_now) {
            glm::mat4 new_local = parent_world_inv * entity_world;
            glm::vec3 t, r, s;
            if (Loom::Math::DecomposeTransform(new_local, t, r, s)) {
                tc.Translation = t;
                tc.Rotation    = r;
                tc.Scale       = s;
            }
        }

        // ── Drag-end: push the undo command if the entity actually changed.
        if (!using_now && mGizmoWasUsing && mDragEntityUUID != 0) {
            // Look the entity up by UUID — the user may have deleted or swapped
            // selection between drag start and end.
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

        // Defensive: keep Tiles sized to map dims (in case scripts/serializer drifted it).
        int expected = tc.Columns * tc.Rows;
        if ((int)tc.Tiles.size() != expected) tc.Tiles.assign(expected, -1);

        glm::vec2 vp_size = mContext.ViewportSize;
        if (vp_size.x <= 0.0f || vp_size.y <= 0.0f) return;

        glm::mat4 view = mContext.EditorCamera.GetViewMatrix();
        glm::mat4 proj = mContext.EditorCamera.GetProjectionMatrix();
        glm::mat4 vp   = proj * view;

        // Tilemap world transform + the plane it lies on (local XY at z=0).
        glm::mat4 world      = mContext.ActiveScene->GetWorldTransform(selected);
        glm::vec3 plane_pt   = glm::vec3(world[3]);
        glm::vec3 plane_n_w  = glm::vec3(world[2]);
        float plane_n_len    = glm::length(plane_n_w);
        if (plane_n_len < 1e-6f) return;
        plane_n_w /= plane_n_len;

        float hw = tc.Columns * tc.TileWidth  * 0.5f;
        float hh = tc.Rows    * tc.TileHeight * 0.5f;

        // Project a tilemap-local point to absolute screen coords (for ImDrawList).
        auto local_to_abs = [&](glm::vec3 local) -> std::optional<ImVec2> {
            glm::vec4 ws = world * glm::vec4(local, 1.0f);
            auto vp_px = Loom::Math::WorldToScreen(glm::vec3(ws), vp_size, vp);
            if (!vp_px) return std::nullopt;
            return ImVec2{ vp_px->x + mContext.ViewportBounds[0].x,
                           vp_px->y + mContext.ViewportBounds[0].y };
        };

        // Window draw list (not the foreground one) so the overlay respects the
        // viewport's clip rect and doesn't bleed over docked panels like Inspector.
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Outer border + interior grid lines (O(rows + cols), not O(rows*cols)).
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

        // Red tint for cells whose sheet tile is flagged Solid — shows the artist exactly
        // which cells will spawn collider rectangles when the scene enters Play.
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

        // Mouse-to-cell.
        if (!mContext.ViewportHovered) return;
        if (ImGui::GetIO().WantTextInput)   return;

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

        // Hovered cell highlight.
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

        // Paint while LMB is held. Idempotent per-cell — we only mutate if the tile changes.
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            int& tile = tc.Tiles[row * tc.Columns + col];
            if (tile != mContext.SelectedTileIndex) {
                tile = mContext.SelectedTileIndex;
                mContext.SceneDirty = true;
            }
        }
    }

} // namespace Weaver
