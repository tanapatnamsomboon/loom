#pragma once

#include "editor_context.h"

namespace Weaver {

    // Unreal-style "World Settings" panel. Shows scene-level properties that
    // get serialized into the .loom file — distinct from editor view-state
    // (grid, fly speed) which lives in the toolbar Settings popup.
    class ScenePropertiesPanel {
    public:
        explicit ScenePropertiesPanel(EditorContext& ctx) : mContext(ctx) {}

        void OnImGuiRender(bool* p_open);

    private:
        void RenderSkyboxSection();

        EditorContext& mContext;
    };

} // namespace Weaver
