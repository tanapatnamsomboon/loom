#include "viewport_panel.h"
#include "editor/commands.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
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
    // File-local helpers
    // ────────────────────────────────────────────────────────────────────────
    namespace {

        constexpr float k_AxisLengthPixels  = 100.0f;   // screen-constant axis length target
        constexpr float k_AxisPickThreshold = 8.0f;     // pixels — proximity for axis-segment hit
        constexpr float k_RingPickThreshold = 6.0f;     // pixels — proximity for rotate ring
        constexpr float k_PlaneSize         = 0.35f;    // fraction of axis length
        constexpr float k_PlaneOffset       = 0.25f;    // fraction of axis length
        constexpr int   k_RingSegments      = 48;       // tessellation per rotate ring

        constexpr ImU32 k_ColorX        = IM_COL32(230,  60,  60, 255);
        constexpr ImU32 k_ColorY        = IM_COL32( 80, 200,  80, 255);
        constexpr ImU32 k_ColorZ        = IM_COL32( 80, 130, 230, 255);
        constexpr ImU32 k_ColorHighlight= IM_COL32(255, 220,  60, 255);
        constexpr ImU32 k_ColorUniform  = IM_COL32(220, 220, 220, 255);
        constexpr ImU32 k_ColorPlaneX   = IM_COL32(230,  60,  60, 110);
        constexpr ImU32 k_ColorPlaneY   = IM_COL32( 80, 200,  80, 110);
        constexpr ImU32 k_ColorPlaneZ   = IM_COL32( 80, 130, 230, 110);

        ImU32 AxisColor(int i, bool highlight) {
            if (highlight) return k_ColorHighlight;
            return (i == 0) ? k_ColorX : (i == 1) ? k_ColorY : k_ColorZ;
        }

        ImU32 PlaneColor(int normal_axis, bool highlight) {
            if (highlight) return (k_ColorHighlight & 0x00FFFFFF) | 0x70000000;
            return (normal_axis == 0) ? k_ColorPlaneX : (normal_axis == 1) ? k_ColorPlaneY : k_ColorPlaneZ;
        }

        glm::vec2 ImVec2ToVec2(const ImVec2& v) { return { v.x, v.y }; }
        ImVec2    Vec2ToImVec2(const glm::vec2& v) { return ImVec2{ v.x, v.y }; }

        // Returns true if `clip` is on the visible side (w > 0) of the camera.
        bool IsInFront(const glm::vec4& clip) { return clip.w > 0.0f; }

        glm::vec2 ProjectToScreen(const glm::vec3& world,
                                  const glm::mat4& vp,
                                  const glm::vec2& viewport_size,
                                  bool& out_in_front) {
            glm::vec4 clip = vp * glm::vec4(world, 1.0f);
            out_in_front = IsInFront(clip);
            if (!out_in_front) return { 0.0f, 0.0f };
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            return {
                (ndc.x * 0.5f + 0.5f) * viewport_size.x,
                (1.0f - (ndc.y * 0.5f + 0.5f)) * viewport_size.y,
            };
        }

        // True if `p` is inside triangle (a,b,c) in 2D.
        bool PointInTriangle2D(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
            auto sign = [](const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2) {
                return (p0.x - p2.x) * (p1.y - p2.y) - (p1.x - p2.x) * (p0.y - p2.y);
            };
            float d1 = sign(p, a, b);
            float d2 = sign(p, b, c);
            float d3 = sign(p, c, a);
            bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
            bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
            return !(neg && pos);
        }

    } // anonymous namespace

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

            // Skybox
            glm::mat4 view    = mContext.EditorCamera.GetViewMatrix();
            view[3]           = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            glm::mat4 skyboxVP = mContext.EditorCamera.GetProjectionMatrix() * view;
            mSkyboxShader->Bind();
            mSkyboxShader->UploadUniformMat4("uViewProjection", skyboxVP);
            Loom::RenderCommand::DrawIndexed(mSkyboxVAO.get(), 36);

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

        RenderGizmo();

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
    // Gizmo — per-frame entry
    // ────────────────────────────────────────────────────────────────────────
    void ViewportPanel::RenderGizmo() {
        mGizmoFrameValid = false;
        mGizmoHover      = GizmoHandle::None;

        // Gates: only Edit mode, only when something is selected, only when op is not None.
        if (mContext.SceneState != SceneState::Edit) {
            if (mGizmoDragging) EndDrag();
            return;
        }
        if (mContext.GizmoOp == GizmoOperation::None) {
            if (mGizmoDragging) EndDrag();
            return;
        }
        Loom::Entity selected = mContext.HierarchyPanel ? mContext.HierarchyPanel->GetSelectedEntity() : Loom::Entity();
        if (!selected || !selected.HasComponent<Loom::TransformComponent>()) {
            if (mGizmoDragging) EndDrag();
            return;
        }

        CacheGizmoFrame();

        // Mouse position in viewport-relative pixels (origin = top-left of viewport image).
        ImVec2    mouse_abs = ImGui::GetMousePos();
        glm::vec2 mouse_vp  = { mouse_abs.x - mContext.ViewportBounds[0].x,
                                mouse_abs.y - mContext.ViewportBounds[0].y };

        // Continue an in-progress drag — runs regardless of frame validity, since drag math
        // anchors on the snapshot taken at drag-start, not the current frame's cache.
        if (mGizmoDragging) {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                EndDrag();
            } else {
                UpdateDrag(mouse_vp);
                CacheGizmoFrame(); // pivot may have moved — refresh for accurate drawing
            }
        }

        if (!mGizmoFrameValid) return; // can't draw or pick without a valid screen-space frame

        // Hover (only when not dragging — keep the active handle locked while dragging).
        if (!mGizmoDragging && mContext.ViewportHovered) {
            mGizmoHover = PickHandleAtMouse(mouse_vp);
        }

        DrawHandles(mGizmoDragging ? mGizmoActive : mGizmoHover);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Gizmo — per-frame cache
    // ────────────────────────────────────────────────────────────────────────
    void ViewportPanel::CacheGizmoFrame() {
        Loom::Entity selected = mContext.HierarchyPanel->GetSelectedEntity();
        if (!selected) return;

        auto& tc = selected.GetComponent<Loom::TransformComponent>();

        // Parent world transform.
        Loom::Entity parent = selected.GetParent();
        mGizmoParentWorld   = parent ? mContext.ActiveScene->GetWorldTransform(parent) : glm::mat4(1.0f);
        mGizmoEntityWorld   = mGizmoParentWorld * tc.GetTransform();
        mGizmoPivotWorld    = glm::vec3(mGizmoEntityWorld[3]);

        // Basis: World mode = identity axes; Local mode = entity's world rotation axes (normalized).
        if (mContext.GizmoMode == GizmoSpace::World) {
            mGizmoBasis[0] = { 1.0f, 0.0f, 0.0f };
            mGizmoBasis[1] = { 0.0f, 1.0f, 0.0f };
            mGizmoBasis[2] = { 0.0f, 0.0f, 1.0f };
        } else {
            glm::vec3 bx = glm::vec3(mGizmoEntityWorld[0]);
            glm::vec3 by = glm::vec3(mGizmoEntityWorld[1]);
            glm::vec3 bz = glm::vec3(mGizmoEntityWorld[2]);
            float lx = glm::length(bx), ly = glm::length(by), lz = glm::length(bz);
            mGizmoBasis[0] = (lx > 1e-6f) ? bx / lx : glm::vec3{ 1.0f, 0.0f, 0.0f };
            mGizmoBasis[1] = (ly > 1e-6f) ? by / ly : glm::vec3{ 0.0f, 1.0f, 0.0f };
            mGizmoBasis[2] = (lz > 1e-6f) ? bz / lz : glm::vec3{ 0.0f, 0.0f, 1.0f };
        }

        mGizmoView         = mContext.EditorCamera.GetViewMatrix();
        mGizmoProj         = mContext.EditorCamera.GetProjectionMatrix();
        mGizmoVP           = mGizmoProj * mGizmoView;
        mGizmoViewportSize = mContext.ViewportSize;
        if (mGizmoViewportSize.x <= 0.0f || mGizmoViewportSize.y <= 0.0f) return;

        // World-space length that projects to k_AxisLengthPixels on screen.
        // For perspective: world_per_pixel = 2 * d * tan(fov/2) / viewport_height.
        // tan(fov/2) = 1 / proj[1][1].
        glm::vec3 cam_pos  = mContext.EditorCamera.GetPosition();
        float distance     = glm::length(mGizmoPivotWorld - cam_pos);
        float inv_proj11   = (mGizmoProj[1][1] != 0.0f) ? (1.0f / mGizmoProj[1][1]) : 1.0f;
        float world_per_px = 2.0f * distance * inv_proj11 / mGizmoViewportSize.y;
        mGizmoWorldAxisLen = world_per_px * k_AxisLengthPixels;
        if (mGizmoWorldAxisLen < 1e-4f) mGizmoWorldAxisLen = 1e-4f;

        bool in_front = false;
        mGizmoPivotScreen = ProjectToScreen(mGizmoPivotWorld, mGizmoVP, mGizmoViewportSize, in_front);
        if (!in_front) return;

        mGizmoSelectionUUID = (uint64_t)selected.GetComponent<Loom::IDComponent>().ID;
        mGizmoFrameValid    = true;
    }

    // ────────────────────────────────────────────────────────────────────────
    // Gizmo — picking
    // ────────────────────────────────────────────────────────────────────────
    ViewportPanel::GizmoHandle ViewportPanel::PickHandleAtMouse(const glm::vec2& mouse_vp) const {
        switch (mContext.GizmoOp) {
            case GizmoOperation::Translate: return PickTranslateHandle(mouse_vp);
            case GizmoOperation::Rotate:    return PickRotateHandle(mouse_vp);
            case GizmoOperation::Scale:     return PickScaleHandle(mouse_vp);
            default:                        return GizmoHandle::None;
        }
    }

    ViewportPanel::GizmoHandle ViewportPanel::PickTranslateHandle(const glm::vec2& mouse_vp) const {
        // Plane handles first (axes can clip through them visually).
        for (int n = 0; n < 3; ++n) {
            int i = (n + 1) % 3;
            int j = (n + 2) % 3;
            glm::vec3 a3 = mGizmoPivotWorld + mGizmoBasis[i] * mGizmoWorldAxisLen * k_PlaneOffset;
            glm::vec3 b3 = a3 + mGizmoBasis[i] * mGizmoWorldAxisLen * k_PlaneSize;
            glm::vec3 c3 = a3 + mGizmoBasis[i] * mGizmoWorldAxisLen * k_PlaneSize
                              + mGizmoBasis[j] * mGizmoWorldAxisLen * k_PlaneSize;
            glm::vec3 d3 = a3 + mGizmoBasis[j] * mGizmoWorldAxisLen * k_PlaneSize;
            bool ok_a, ok_b, ok_c, ok_d;
            glm::vec2 a = ProjectToScreen(a3, mGizmoVP, mGizmoViewportSize, ok_a);
            glm::vec2 b = ProjectToScreen(b3, mGizmoVP, mGizmoViewportSize, ok_b);
            glm::vec2 c = ProjectToScreen(c3, mGizmoVP, mGizmoViewportSize, ok_c);
            glm::vec2 d = ProjectToScreen(d3, mGizmoVP, mGizmoViewportSize, ok_d);
            if (!(ok_a && ok_b && ok_c && ok_d)) continue;
            if (PointInTriangle2D(mouse_vp, a, b, c) || PointInTriangle2D(mouse_vp, a, c, d)) {
                switch (n) {
                    case 0: return GizmoHandle::PlaneYZ;
                    case 1: return GizmoHandle::PlaneXZ;
                    case 2: return GizmoHandle::PlaneXY;
                }
            }
        }
        // Axes.
        float    best_dist = k_AxisPickThreshold;
        int      best_axis = -1;
        for (int i = 0; i < 3; ++i) {
            glm::vec3 tip3 = mGizmoPivotWorld + mGizmoBasis[i] * mGizmoWorldAxisLen;
            bool      ok   = false;
            glm::vec2 tip2 = ProjectToScreen(tip3, mGizmoVP, mGizmoViewportSize, ok);
            if (!ok) continue;
            float d = Loom::Math::DistancePointToSegment2D(mouse_vp, mGizmoPivotScreen, tip2);
            if (d < best_dist) { best_dist = d; best_axis = i; }
        }
        if (best_axis == 0) return GizmoHandle::AxisX;
        if (best_axis == 1) return GizmoHandle::AxisY;
        if (best_axis == 2) return GizmoHandle::AxisZ;
        return GizmoHandle::None;
    }

    ViewportPanel::GizmoHandle ViewportPanel::PickRotateHandle(const glm::vec2& mouse_vp) const {
        float radius_world = mGizmoWorldAxisLen;
        float best_dist    = k_RingPickThreshold;
        int   best_axis    = -1;

        for (int axis = 0; axis < 3; ++axis) {
            int i = (axis + 1) % 3;
            int j = (axis + 2) % 3;
            const glm::vec3& u = mGizmoBasis[i];
            const glm::vec3& v = mGizmoBasis[j];

            // Sample ring in screen space, find nearest segment.
            ImVec2 prev{};
            bool   prev_ok = false;
            for (int s = 0; s <= k_RingSegments; ++s) {
                float a   = (float)s / (float)k_RingSegments * glm::two_pi<float>();
                glm::vec3 p3 = mGizmoPivotWorld + (u * std::cos(a) + v * std::sin(a)) * radius_world;
                bool ok;
                glm::vec2 p2 = ProjectToScreen(p3, mGizmoVP, mGizmoViewportSize, ok);
                if (s > 0 && prev_ok && ok) {
                    float d = Loom::Math::DistancePointToSegment2D(mouse_vp, ImVec2ToVec2(prev), p2);
                    if (d < best_dist) { best_dist = d; best_axis = axis; }
                }
                prev    = Vec2ToImVec2(p2);
                prev_ok = ok;
            }
        }
        if (best_axis == 0) return GizmoHandle::RingX;
        if (best_axis == 1) return GizmoHandle::RingY;
        if (best_axis == 2) return GizmoHandle::RingZ;
        return GizmoHandle::None;
    }

    ViewportPanel::GizmoHandle ViewportPanel::PickScaleHandle(const glm::vec2& mouse_vp) const {
        // Uniform-scale center handle first.
        if (glm::length(mouse_vp - mGizmoPivotScreen) < 10.0f)
            return GizmoHandle::ScaleUniform;

        float best_dist = k_AxisPickThreshold;
        int   best_axis = -1;
        for (int i = 0; i < 3; ++i) {
            glm::vec3 tip3 = mGizmoPivotWorld + mGizmoBasis[i] * mGizmoWorldAxisLen;
            bool      ok   = false;
            glm::vec2 tip2 = ProjectToScreen(tip3, mGizmoVP, mGizmoViewportSize, ok);
            if (!ok) continue;
            float d = Loom::Math::DistancePointToSegment2D(mouse_vp, mGizmoPivotScreen, tip2);
            if (d < best_dist) { best_dist = d; best_axis = i; }
        }
        if (best_axis == 0) return GizmoHandle::ScaleX;
        if (best_axis == 1) return GizmoHandle::ScaleY;
        if (best_axis == 2) return GizmoHandle::ScaleZ;
        return GizmoHandle::None;
    }

    // ────────────────────────────────────────────────────────────────────────
    // Gizmo — drawing
    // ────────────────────────────────────────────────────────────────────────
    void ViewportPanel::DrawHandles(GizmoHandle highlight) const {
        ImDrawList* dl       = ImGui::GetWindowDrawList();
        ImVec2      vp_origin{ mContext.ViewportBounds[0].x, mContext.ViewportBounds[0].y };

        auto to_abs = [&](const glm::vec2& vp) -> ImVec2 {
            return { vp_origin.x + vp.x, vp_origin.y + vp.y };
        };

        // Translate ───────────────────────────────────────────────────────
        if (mContext.GizmoOp == GizmoOperation::Translate) {
            // Plane handles (under axes)
            for (int n = 0; n < 3; ++n) {
                int i = (n + 1) % 3;
                int j = (n + 2) % 3;
                glm::vec3 a3 = mGizmoPivotWorld + mGizmoBasis[i] * mGizmoWorldAxisLen * k_PlaneOffset;
                glm::vec3 b3 = a3 + mGizmoBasis[i] * mGizmoWorldAxisLen * k_PlaneSize;
                glm::vec3 c3 = a3 + mGizmoBasis[i] * mGizmoWorldAxisLen * k_PlaneSize
                                  + mGizmoBasis[j] * mGizmoWorldAxisLen * k_PlaneSize;
                glm::vec3 d3 = a3 + mGizmoBasis[j] * mGizmoWorldAxisLen * k_PlaneSize;
                bool ok_a, ok_b, ok_c, ok_d;
                glm::vec2 a = ProjectToScreen(a3, mGizmoVP, mGizmoViewportSize, ok_a);
                glm::vec2 b = ProjectToScreen(b3, mGizmoVP, mGizmoViewportSize, ok_b);
                glm::vec2 c = ProjectToScreen(c3, mGizmoVP, mGizmoViewportSize, ok_c);
                glm::vec2 d = ProjectToScreen(d3, mGizmoVP, mGizmoViewportSize, ok_d);
                if (!(ok_a && ok_b && ok_c && ok_d)) continue;

                GizmoHandle this_handle = (n == 0) ? GizmoHandle::PlaneYZ
                                       : (n == 1) ? GizmoHandle::PlaneXZ
                                                  : GizmoHandle::PlaneXY;
                bool h    = (highlight == this_handle);
                ImU32 col = PlaneColor(n, h);
                dl->AddQuadFilled(to_abs(a), to_abs(b), to_abs(c), to_abs(d), col);
                dl->AddQuad      (to_abs(a), to_abs(b), to_abs(c), to_abs(d), (col | 0xFF000000), 1.5f);
            }
            // Axes
            for (int i = 0; i < 3; ++i) {
                glm::vec3 tip3 = mGizmoPivotWorld + mGizmoBasis[i] * mGizmoWorldAxisLen;
                bool      ok   = false;
                glm::vec2 tip2 = ProjectToScreen(tip3, mGizmoVP, mGizmoViewportSize, ok);
                if (!ok) continue;
                GizmoHandle this_handle = (i == 0) ? GizmoHandle::AxisX
                                       : (i == 1) ? GizmoHandle::AxisY
                                                  : GizmoHandle::AxisZ;
                ImU32 col = AxisColor(i, highlight == this_handle);
                dl->AddLine(to_abs(mGizmoPivotScreen), to_abs(tip2), col, 3.0f);
                dl->AddCircleFilled(to_abs(tip2), 5.0f, col);
            }
        }
        // Rotate ──────────────────────────────────────────────────────────
        else if (mContext.GizmoOp == GizmoOperation::Rotate) {
            float radius_world = mGizmoWorldAxisLen;
            for (int axis = 0; axis < 3; ++axis) {
                int i = (axis + 1) % 3;
                int j = (axis + 2) % 3;
                const glm::vec3& u = mGizmoBasis[i];
                const glm::vec3& v = mGizmoBasis[j];
                GizmoHandle this_handle = (axis == 0) ? GizmoHandle::RingX
                                       : (axis == 1) ? GizmoHandle::RingY
                                                     : GizmoHandle::RingZ;
                ImU32 col = AxisColor(axis, highlight == this_handle);

                ImVec2 prev{};
                bool   prev_ok = false;
                for (int s = 0; s <= k_RingSegments; ++s) {
                    float a    = (float)s / (float)k_RingSegments * glm::two_pi<float>();
                    glm::vec3 p3 = mGizmoPivotWorld + (u * std::cos(a) + v * std::sin(a)) * radius_world;
                    bool ok;
                    glm::vec2 p2 = ProjectToScreen(p3, mGizmoVP, mGizmoViewportSize, ok);
                    if (s > 0 && prev_ok && ok)
                        dl->AddLine(prev, to_abs(p2), col, (highlight == this_handle) ? 3.0f : 2.0f);
                    prev    = to_abs(p2);
                    prev_ok = ok;
                }
            }
        }
        // Scale ───────────────────────────────────────────────────────────
        else if (mContext.GizmoOp == GizmoOperation::Scale) {
            // Uniform center
            ImU32 uniform_col = (highlight == GizmoHandle::ScaleUniform) ? k_ColorHighlight : k_ColorUniform;
            dl->AddCircleFilled(to_abs(mGizmoPivotScreen), 7.0f, uniform_col);

            for (int i = 0; i < 3; ++i) {
                glm::vec3 tip3 = mGizmoPivotWorld + mGizmoBasis[i] * mGizmoWorldAxisLen;
                bool      ok   = false;
                glm::vec2 tip2 = ProjectToScreen(tip3, mGizmoVP, mGizmoViewportSize, ok);
                if (!ok) continue;
                GizmoHandle this_handle = (i == 0) ? GizmoHandle::ScaleX
                                       : (i == 1) ? GizmoHandle::ScaleY
                                                  : GizmoHandle::ScaleZ;
                ImU32 col = AxisColor(i, highlight == this_handle);
                dl->AddLine(to_abs(mGizmoPivotScreen), to_abs(tip2), col, 3.0f);
                // Cube tip (square in screen space).
                ImVec2 t = to_abs(tip2);
                dl->AddRectFilled({ t.x - 5.0f, t.y - 5.0f }, { t.x + 5.0f, t.y + 5.0f }, col);
            }
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // Gizmo — interaction
    // ────────────────────────────────────────────────────────────────────────
    bool ViewportPanel::BeginGizmoDragIfHovered() {
        if (!mGizmoFrameValid) return false;
        if (mContext.SceneState != SceneState::Edit) return false;

        // Re-pick at click time (mouse may have moved since the last frame's hover sweep).
        ImVec2    mouse_abs = ImGui::GetMousePos();
        glm::vec2 mouse_vp{ mouse_abs.x - mContext.ViewportBounds[0].x,
                            mouse_abs.y - mContext.ViewportBounds[0].y };
        GizmoHandle handle = PickHandleAtMouse(mouse_vp);
        if (handle == GizmoHandle::None) return false;

        Loom::Entity selected = mContext.HierarchyPanel ? mContext.HierarchyPanel->GetSelectedEntity() : Loom::Entity();
        if (!selected || !selected.HasComponent<Loom::TransformComponent>()) return false;

        mGizmoHover         = handle;
        mGizmoActive        = handle;
        mGizmoDragging      = true;
        mDragEntityUUID     = (uint64_t)selected.GetComponent<Loom::IDComponent>().ID;
        mDragStartLocal     = selected.GetComponent<Loom::TransformComponent>();
        mDragStartParentWorld    = mGizmoParentWorld;
        mDragStartParentWorldInv = glm::inverse(mGizmoParentWorld);
        mDragStartEntityWorld    = mGizmoEntityWorld;
        mDragStartPivotWorld     = mGizmoPivotWorld;
        for (int i = 0; i < 3; ++i) mDragStartBasis[i] = mGizmoBasis[i];

        // Initial mouse-derived state by handle kind. (mouse_vp already computed above)
        mDragStartMouse = mouse_vp;

        Loom::Math::Ray ray = Loom::Math::ScreenToRay(mouse_vp, mGizmoViewportSize, mGizmoView, mGizmoProj);
        glm::vec3 cam_pos   = mContext.EditorCamera.GetPosition();

        auto pick_axis_plane_normal = [&](const glm::vec3& axis) {
            // Pick the plane containing `axis` whose normal is most camera-facing.
            glm::vec3 view_dir = glm::normalize(cam_pos - mGizmoPivotWorld);
            glm::vec3 ortho    = glm::cross(axis, view_dir);
            if (glm::length(ortho) < 1e-4f) {
                // Axis is parallel to view direction — fall back to any perpendicular.
                ortho = glm::cross(axis, glm::vec3(0.0f, 1.0f, 0.0f));
                if (glm::length(ortho) < 1e-4f)
                    ortho = glm::cross(axis, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            glm::vec3 normal = glm::normalize(glm::cross(axis, ortho));
            return normal;
        };

        switch (mGizmoActive) {
            case GizmoHandle::AxisX:
            case GizmoHandle::AxisY:
            case GizmoHandle::AxisZ:
            case GizmoHandle::ScaleX:
            case GizmoHandle::ScaleY:
            case GizmoHandle::ScaleZ: {
                int axis_i = 0;
                if (mGizmoActive == GizmoHandle::AxisY || mGizmoActive == GizmoHandle::ScaleY) axis_i = 1;
                if (mGizmoActive == GizmoHandle::AxisZ || mGizmoActive == GizmoHandle::ScaleZ) axis_i = 2;
                mDragStartAxis        = mGizmoBasis[axis_i];
                mDragStartPlaneNormal = pick_axis_plane_normal(mDragStartAxis);
                if (auto hit = Loom::Math::RayPlaneIntersect(ray, mGizmoPivotWorld, mDragStartPlaneNormal)) {
                    mDragStartHitWorld = *hit;
                    glm::vec3 to_hit   = mDragStartHitWorld - mGizmoPivotWorld;
                    mDragStartAxisProj = glm::dot(to_hit, mDragStartAxis);
                } else {
                    mDragStartHitWorld = mGizmoPivotWorld;
                    mDragStartAxisProj = 0.0f;
                }
                break;
            }
            case GizmoHandle::PlaneXY:
            case GizmoHandle::PlaneYZ:
            case GizmoHandle::PlaneXZ: {
                int normal_i = (mGizmoActive == GizmoHandle::PlaneYZ) ? 0
                            : (mGizmoActive == GizmoHandle::PlaneXZ) ? 1 : 2;
                mDragStartPlaneNormal = mGizmoBasis[normal_i];
                if (auto hit = Loom::Math::RayPlaneIntersect(ray, mGizmoPivotWorld, mDragStartPlaneNormal))
                    mDragStartHitWorld = *hit;
                else
                    mDragStartHitWorld = mGizmoPivotWorld;
                break;
            }
            case GizmoHandle::RingX:
            case GizmoHandle::RingY:
            case GizmoHandle::RingZ: {
                int axis_i = (mGizmoActive == GizmoHandle::RingX) ? 0
                          : (mGizmoActive == GizmoHandle::RingY) ? 1 : 2;
                mDragStartAxis        = mGizmoBasis[axis_i];
                mDragStartPlaneNormal = mDragStartAxis; // rotation plane is perpendicular to axis
                if (auto hit = Loom::Math::RayPlaneIntersect(ray, mGizmoPivotWorld, mDragStartPlaneNormal)) {
                    mDragStartHitWorld = *hit;
                    glm::vec3 v = *hit - mGizmoPivotWorld;
                    int i = (axis_i + 1) % 3;
                    int j = (axis_i + 2) % 3;
                    float vx = glm::dot(v, mGizmoBasis[i]);
                    float vy = glm::dot(v, mGizmoBasis[j]);
                    mDragStartAngle = std::atan2(vy, vx);
                } else {
                    mDragStartAngle = 0.0f;
                }
                break;
            }
            case GizmoHandle::ScaleUniform:
                // Use mouse delta from pivot screen position as 1.0× reference.
                mDragStartAxisProj = std::max(glm::length(mouse_vp - mGizmoPivotScreen), 1.0f);
                break;
            default: break;
        }
        return true;
    }

    void ViewportPanel::UpdateDrag(const glm::vec2& mouse_vp) {
        Loom::Entity e = mContext.ActiveScene->GetEntityByUUID(Loom::UUID(mDragEntityUUID));
        if (!e || !e.HasComponent<Loom::TransformComponent>()) return;

        Loom::Math::Ray ray = Loom::Math::ScreenToRay(mouse_vp, mGizmoViewportSize, mGizmoView, mGizmoProj);
        auto& tc = e.GetComponent<Loom::TransformComponent>();

        auto apply_world_translation = [&](const glm::vec3& world_delta) {
            glm::vec4 local_delta = mDragStartParentWorldInv * glm::vec4(world_delta, 0.0f);
            tc.Translation = mDragStartLocal.Translation + glm::vec3(local_delta);
        };

        switch (mGizmoActive) {
            case GizmoHandle::AxisX:
            case GizmoHandle::AxisY:
            case GizmoHandle::AxisZ: {
                auto hit = Loom::Math::RayPlaneIntersect(ray, mDragStartPivotWorld, mDragStartPlaneNormal);
                if (!hit) return;
                float    proj  = glm::dot(*hit - mDragStartPivotWorld, mDragStartAxis);
                float    delta = proj - mDragStartAxisProj;
                glm::vec3 world_delta = mDragStartAxis * delta;
                apply_world_translation(world_delta);
                break;
            }
            case GizmoHandle::PlaneXY:
            case GizmoHandle::PlaneYZ:
            case GizmoHandle::PlaneXZ: {
                auto hit = Loom::Math::RayPlaneIntersect(ray, mDragStartPivotWorld, mDragStartPlaneNormal);
                if (!hit) return;
                glm::vec3 world_delta = *hit - mDragStartHitWorld;
                apply_world_translation(world_delta);
                break;
            }
            case GizmoHandle::RingX:
            case GizmoHandle::RingY:
            case GizmoHandle::RingZ: {
                auto hit = Loom::Math::RayPlaneIntersect(ray, mDragStartPivotWorld, mDragStartPlaneNormal);
                if (!hit) return;
                int axis_i = (mGizmoActive == GizmoHandle::RingX) ? 0
                          : (mGizmoActive == GizmoHandle::RingY) ? 1 : 2;
                int i = (axis_i + 1) % 3;
                int j = (axis_i + 2) % 3;
                glm::vec3 v   = *hit - mDragStartPivotWorld;
                float vx      = glm::dot(v, mDragStartBasis[i]);
                float vy      = glm::dot(v, mDragStartBasis[j]);
                float angle   = std::atan2(vy, vx);
                float delta_a = angle - mDragStartAngle;
                // Build rotation in world space around the drag axis through the pivot.
                glm::mat4 R = glm::rotate(glm::mat4(1.0f), delta_a, mDragStartAxis);
                glm::mat4 T_pivot     = glm::translate(glm::mat4(1.0f),  mDragStartPivotWorld);
                glm::mat4 T_pivot_inv = glm::translate(glm::mat4(1.0f), -mDragStartPivotWorld);
                glm::mat4 new_world   = T_pivot * R * T_pivot_inv * mDragStartEntityWorld;
                Loom::TransformComponent new_local = ApplyDeltaToLocal(mDragStartLocal, new_world);
                tc = new_local;
                break;
            }
            case GizmoHandle::ScaleX:
            case GizmoHandle::ScaleY:
            case GizmoHandle::ScaleZ: {
                auto hit = Loom::Math::RayPlaneIntersect(ray, mDragStartPivotWorld, mDragStartPlaneNormal);
                if (!hit) return;
                int axis_i = (mGizmoActive == GizmoHandle::ScaleX) ? 0
                          : (mGizmoActive == GizmoHandle::ScaleY) ? 1 : 2;
                float proj = glm::dot(*hit - mDragStartPivotWorld, mDragStartAxis);
                float base = (std::abs(mDragStartAxisProj) > 1e-4f) ? mDragStartAxisProj : mGizmoWorldAxisLen;
                float ratio = proj / base;
                if (!std::isfinite(ratio)) ratio = 1.0f;
                tc.Scale            = mDragStartLocal.Scale;
                tc.Scale[axis_i]    = mDragStartLocal.Scale[axis_i] * ratio;
                break;
            }
            case GizmoHandle::ScaleUniform: {
                float dist  = std::max(glm::length(mouse_vp - mGizmoPivotScreen), 1.0f);
                float ratio = dist / mDragStartAxisProj;
                if (!std::isfinite(ratio) || ratio < 0.01f) ratio = 0.01f;
                tc.Scale = mDragStartLocal.Scale * ratio;
                break;
            }
            default: break;
        }
    }

    void ViewportPanel::EndDrag() {
        if (!mGizmoDragging) {
            mGizmoActive = GizmoHandle::None;
            return;
        }

        Loom::Entity e = mContext.ActiveScene->GetEntityByUUID(Loom::UUID(mDragEntityUUID));
        if (e && e.HasComponent<Loom::TransformComponent>()) {
            Loom::TransformComponent after = e.GetComponent<Loom::TransformComponent>();

            // Only record an undo entry if the transform actually changed.
            const glm::vec3 dt = after.Translation - mDragStartLocal.Translation;
            const glm::vec3 dr = after.Rotation    - mDragStartLocal.Rotation;
            const glm::vec3 ds = after.Scale       - mDragStartLocal.Scale;
            const float    eps = 1e-5f;
            bool changed = glm::length(dt) > eps || glm::length(dr) > eps || glm::length(ds) > eps;

            if (changed) {
                const char* desc = (mContext.GizmoOp == GizmoOperation::Translate) ? "Translate"
                                : (mContext.GizmoOp == GizmoOperation::Rotate)    ? "Rotate"
                                                                                  : "Scale";
                // Revert to before-state, then push command — Push() executes the after-state.
                e.GetComponent<Loom::TransformComponent>() = mDragStartLocal;
                mContext.History.Push(std::make_unique<TransformEditCommand>(
                    mContext.ActiveScene,
                    Loom::UUID(mDragEntityUUID),
                    mDragStartLocal,
                    after,
                    desc));
            }
        }

        mGizmoDragging = false;
        mGizmoActive   = GizmoHandle::None;
    }

    // ────────────────────────────────────────────────────────────────────────
    // Transform-component composition: world → entity-local
    // ────────────────────────────────────────────────────────────────────────
    Loom::TransformComponent ViewportPanel::ApplyDeltaToLocal(const Loom::TransformComponent& start_local,
                                                              const glm::mat4& new_world) const {
        glm::mat4 new_local_mat = mDragStartParentWorldInv * new_world;
        glm::vec3 t, r, s;
        if (!Loom::Math::DecomposeTransform(new_local_mat, t, r, s)) {
            return start_local;
        }
        Loom::TransformComponent out = start_local;
        out.Translation = t;
        out.Rotation    = r;
        out.Scale       = s;
        return out;
    }

} // namespace Weaver
