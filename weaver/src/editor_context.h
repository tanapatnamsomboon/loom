#pragma once

#include "editor/editor_history.h"
#include "panels/scene_hierarchy_panel.h"
#include <glm/glm.hpp>
#include <loom/renderer/editor_camera.h>
#include <loom/scene/entity.h>
#include <loom/scene/scene.h>
#include <memory>
#include <string>

namespace Weaver {

    enum class SceneState {
        Edit = 0,
        Play = 1
    };

    enum class GizmoOperation {
        None      = 0,
        Translate = 1,
        Rotate    = 2,
        Scale     = 3,
    };

    enum class GizmoSpace {
        World = 0,
        Local = 1,
    };

    struct GridSettings {
        float MinorScale     = 1.0f;
        float MajorScale     = 10.0f;
        float LineThickness  = 1.0f;
        float FadeStart      = 20.0f;
        float FadeEnd        = 80.0f;
        glm::vec4 MinorColor = { 0.5f, 0.5f, 0.5f, 0.5f };
        glm::vec4 MajorColor = { 0.7f, 0.7f, 0.7f, 0.7f };
    };

    // Shared mutable state passed by reference to all editor subsystems.
    // Only cross-cutting data lives here; each subsystem owns its private state.
    struct EditorContext {
        // Scene lifecycle
        SceneState                   SceneState = SceneState::Edit;
        std::shared_ptr<Loom::Scene> EditorScene;
        std::shared_ptr<Loom::Scene> ActiveScene;
        std::string                  CurrentScenePath;
        bool                         SceneDirty = false; // true for edits outside the history (e.g. tag renames, prefab drops)

        // Undo / Redo history
        EditorHistory History;

        bool IsDirty() const { return SceneDirty || History.IsDirty(); }

        // Camera
        Loom::EditorCamera EditorCamera;

        // Viewport geometry — written by ViewportPanel, read by ToolbarPanel & input handlers
        glm::vec2 ViewportBounds[2] = {};
        glm::vec2 ViewportSize      = { 0.0f, 0.0f };
        bool      ViewportHovered   = false;
        bool      ViewportFocused   = false;

        // Entity picking
        Loom::Entity HoveredEntity;

        // Grid visual settings — written by ToolbarPanel, read by ViewportPanel
        GridSettings Grid;

        // Gizmo state — written by ToolbarPanel + keyboard shortcuts, read by ViewportPanel
        GizmoOperation GizmoOp    = GizmoOperation::Translate;
        GizmoSpace     GizmoMode  = GizmoSpace::Local;

        // Non-owning pointer to the scene hierarchy panel (owned by EditorLayer)
        SceneHierarchyPanel* HierarchyPanel = nullptr;
    };

} // namespace Weaver
