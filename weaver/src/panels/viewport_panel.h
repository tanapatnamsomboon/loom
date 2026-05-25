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
        // Picks the rendered sub-rect inside the panel — full panel except in
        // Play mode with a fixed-aspect primary camera, where it letterboxes.
        void ComputeGameViewRect();

        // ── Tile paint ─────────────────────────────────────────────────────
        void RenderTilePaint();

        // ── Gizmo (ImGuizmo) ───────────────────────────────────────────────
        // Per-frame ImGuizmo wiring; batches the full drag into one undo step.
        void RenderGizmo();

    private:
        EditorContext& mContext;

        std::function<void(const std::filesystem::path&)> mSceneOpenCallback;
        std::function<void(const std::filesystem::path&)> mPrefabInstantiateCallback;

        // Pipeline: HDR scene (RGBA16F + RED_INTEGER picking + DEPTH) → tonemap →
        // LDR (RGBA8) → FXAA → Final (RGBA8) shown by ImGui::Image.
        std::shared_ptr<Loom::Framebuffer>  mFramebuffer;
        std::shared_ptr<Loom::Framebuffer>  mLDRFramebuffer;
        std::shared_ptr<Loom::Framebuffer>  mFinalFramebuffer;

        std::shared_ptr<Loom::VertexArray>  mGridVAO;
        std::shared_ptr<Loom::VertexBuffer> mGridVBO;
        std::shared_ptr<Loom::Shader>       mGridShader;

        bool                     mGizmoWasUsing  = false; // drag edge detection
        uint64_t                 mDragEntityUUID = 0;
        Loom::TransformComponent mDragStartLocal;

        // Scene framebuffer size + offset inside the panel (for play-mode letterbox).
        glm::vec2 mGameViewSize   = { 0.0f, 0.0f };
        glm::vec2 mGameViewOffset = { 0.0f, 0.0f };
    };

} // namespace Weaver
