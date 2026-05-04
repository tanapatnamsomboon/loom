#pragma once

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
        SceneState                   SceneState   = SceneState::Edit;
        std::shared_ptr<Loom::Scene> EditorScene;
        std::shared_ptr<Loom::Scene> ActiveScene;
        std::string                  CurrentScenePath;
        bool                         SceneDirty = false;

        // Camera
        Loom::EditorCamera EditorCamera;

        // Viewport geometry — written by ViewportPanel, read by ToolbarPanel & input handlers
        glm::vec2 ViewportBounds[2] = {};
        glm::vec2 ViewportSize      = { 0.0f, 0.0f };
        bool      ViewportHovered   = false;
        bool      ViewportFocused   = false;

        // Entity picking
        Loom::Entity HoveredEntity;

        // Gizmos
        int GizmoType = -1;
        int GizmoMode = 0; // 0 = Local, 1 = World

        // Grid visual settings — written by ToolbarPanel, read by ViewportPanel
        GridSettings Grid;

        // Non-owning pointer to the scene hierarchy panel (owned by EditorLayer)
        SceneHierarchyPanel* HierarchyPanel = nullptr;
    };

} // namespace Weaver
