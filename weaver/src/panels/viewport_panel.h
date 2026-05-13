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

        // Called by EditorLayer on left-mouse-down. Returns true if the click
        // landed on a gizmo handle (the click is consumed; selection is skipped).
        bool BeginGizmoDragIfHovered();

        // True while the user is actively dragging a gizmo handle.
        bool IsGizmoDragging() const { return mGizmoDragging; }

    private:
        // ── Gizmo handle taxonomy ──────────────────────────────────────────
        enum class GizmoHandle {
            None = 0,
            // Translate
            AxisX, AxisY, AxisZ,
            PlaneXY, PlaneYZ, PlaneXZ,
            // Rotate
            RingX, RingY, RingZ,
            // Scale
            ScaleX, ScaleY, ScaleZ,
            ScaleUniform,
        };

        // ── Setup ──────────────────────────────────────────────────────────
        void HandleViewportResize();
        void UpdateViewportBounds();
        void UpdateViewportSize();

        // ── Gizmo (all defined in viewport_panel.cpp) ──────────────────────
        void        RenderGizmo();                    // entry — called from OnImGuiRender
        void        CacheGizmoFrame();                // recomputes pivot/basis/projection every frame
        GizmoHandle PickHandleAtMouse(const glm::vec2& mouse_vp) const;
        void        DrawHandles(GizmoHandle hovered) const;
        void        UpdateDrag(const glm::vec2& mouse_vp);
        void        EndDrag();

        // Picking helpers
        GizmoHandle PickTranslateHandle(const glm::vec2& mouse_vp) const;
        GizmoHandle PickRotateHandle   (const glm::vec2& mouse_vp) const;
        GizmoHandle PickScaleHandle    (const glm::vec2& mouse_vp) const;

        // Returns the entity-local TransformComponent corresponding to a world-space update.
        Loom::TransformComponent ApplyDeltaToLocal(const Loom::TransformComponent& start_local,
                                                   const glm::mat4& new_world) const;

    private:
        EditorContext& mContext;

        std::function<void(const std::filesystem::path&)> mSceneOpenCallback;
        std::function<void(const std::filesystem::path&)> mPrefabInstantiateCallback;

        std::shared_ptr<Loom::Framebuffer>  mFramebuffer;

        std::shared_ptr<Loom::VertexArray>  mSkyboxVAO;
        std::shared_ptr<Loom::VertexBuffer> mSkyboxVBO;
        std::shared_ptr<Loom::Shader>       mSkyboxShader;

        std::shared_ptr<Loom::VertexArray>  mGridVAO;
        std::shared_ptr<Loom::VertexBuffer> mGridVBO;
        std::shared_ptr<Loom::Shader>       mGridShader;

        // ── Gizmo per-frame cache (recomputed each OnImGuiRender) ──────────
        bool      mGizmoFrameValid = false;
        glm::mat4 mGizmoView           = glm::mat4(1.0f);
        glm::mat4 mGizmoProj           = glm::mat4(1.0f);
        glm::mat4 mGizmoVP             = glm::mat4(1.0f);
        glm::vec2 mGizmoViewportSize   = { 0.0f, 0.0f };
        glm::vec3 mGizmoPivotWorld     = { 0.0f, 0.0f, 0.0f };
        glm::vec2 mGizmoPivotScreen    = { 0.0f, 0.0f };
        glm::vec3 mGizmoBasis[3]       = { {1,0,0}, {0,1,0}, {0,0,1} };
        glm::mat4 mGizmoParentWorld    = glm::mat4(1.0f);
        glm::mat4 mGizmoEntityWorld    = glm::mat4(1.0f);
        float     mGizmoWorldAxisLen   = 1.0f;
        uint64_t  mGizmoSelectionUUID  = 0;

        // ── Gizmo interaction state ────────────────────────────────────────
        GizmoHandle mGizmoHover    = GizmoHandle::None;
        GizmoHandle mGizmoActive   = GizmoHandle::None;
        bool        mGizmoDragging = false;

        // Drag-start snapshots
        uint64_t                 mDragEntityUUID    = 0;
        Loom::TransformComponent mDragStartLocal;
        glm::mat4                mDragStartEntityWorld    = glm::mat4(1.0f);
        glm::mat4                mDragStartParentWorld    = glm::mat4(1.0f);
        glm::mat4                mDragStartParentWorldInv = glm::mat4(1.0f);
        glm::vec3                mDragStartPivotWorld     = { 0.0f, 0.0f, 0.0f };
        glm::vec3                mDragStartBasis[3]       = { {1,0,0}, {0,1,0}, {0,0,1} };
        glm::vec3                mDragStartHitWorld       = { 0.0f, 0.0f, 0.0f };
        glm::vec3                mDragStartAxis           = { 1.0f, 0.0f, 0.0f };
        glm::vec3                mDragStartPlaneNormal    = { 0.0f, 0.0f, 1.0f };
        float                    mDragStartAxisProj       = 0.0f;
        float                    mDragStartAngle          = 0.0f;
        glm::vec2                mDragStartMouse          = { 0.0f, 0.0f };
    };

} // namespace Weaver
