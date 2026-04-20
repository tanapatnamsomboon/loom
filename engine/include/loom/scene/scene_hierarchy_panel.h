#pragma once

#include "loom/core/core.h"
#include "loom/scene/scene.h"
#include "loom/scene/entity.h"

namespace Loom {

    class LOOM_API SceneHierarchyPanel {
    public:
        SceneHierarchyPanel() = default;
        SceneHierarchyPanel(const std::shared_ptr<Scene>& context);

        void SetContext(const std::shared_ptr<Scene>& context);

        void OnImGuiRender();

    private:
        void DrawEntityNode(Entity entity);
        void DrawComponents(Entity entity);

    private:
        std::shared_ptr<Scene> mContext;
        Entity mSelectionContext;
    };

}