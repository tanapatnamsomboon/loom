#pragma once

#include "editor_context.h"
#include <glm/glm.hpp>
#include <loom/core/timestep.h>
#include <loom/math/math.h>
#include <loom/renderer/buffer.h>
#include <loom/renderer/framebuffer.h>
#include <loom/renderer/shader.h>
#include <loom/renderer/vertex_array.h>
#include <loom/scene/components.h>
#include <loom/scene/entity.h>
#include <filesystem>
#include <functional>

namespace Weaver {

    class ViewportPanel {
    public:
        explicit ViewportPanel(EditorContext& ctx);

        void Init();

        void SetSceneOpenCallback(const std::function<void(const std::filesystem::path&)>& callback) { mSceneOpenCallback = callback; }
        void SetPrefabInstantiateCallback(const std::function<void(const std::filesystem::path&)>& callback) { mPrefabInstantiateCallback = callback; }

        void BeginFrame();
        void RenderScene(Loom::Timestep ts);
        void UpdateHoveredEntity();
        void EndFrame();

        void OnImGuiRender();

        // True while the cursor is over an ImGuizmo handle or actively dragging
        // one. EditorLayer reads this to skip entity selection on LMB-down.
        bool IsGizmoBusy() const;

        // True while the tile paint tool is the active mouse tool and the selected
        // entity has a TilemapComponent — viewport clicks paint instead of selecting.
        bool IsTilePaintActive() const;

    private:
        // ── Setup ──────────────────────────────────────────────────────────
        void HandleViewportResize();
        void UpdateViewportBounds();
        void UpdateViewportSize();

        // ── Tile paint ─────────────────────────────────────────────────────
        void RenderTilePaint();

        // ── Gizmo (ImGuizmo) ───────────────────────────────────────────────
        // RenderGizmo runs every OnImGuiRender. It sets up ImGuizmo's per-frame
        // viewport rect, calls ImGuizmo::Manipulate, and on drag-end pushes a
        // TransformEditCommand so the move/rotate/scale lands in undo history
        // as one batched step (not per-frame).
        void RenderGizmo();

    private:
        EditorContext& mContext;

        std::function<void(const std::filesystem::path&)> mSceneOpenCallback;
        std::function<void(const std::filesystem::path&)> mPrefabInstantiateCallback;

        std::shared_ptr<Loom::Framebuffer>  mFramebuffer;

        std::shared_ptr<Loom::VertexArray>  mGridVAO;
        std::shared_ptr<Loom::VertexBuffer> mGridVBO;
        std::shared_ptr<Loom::Shader>       mGridShader;

        // ── Gizmo state (just enough to batch drag deltas into one undo step)
        bool                     mGizmoWasUsing  = false; // ImGuizmo::IsUsing() last frame, to detect drag start/end edges
        uint64_t                 mDragEntityUUID = 0;
        Loom::TransformComponent mDragStartLocal;
    };

} // namespace Weaver
