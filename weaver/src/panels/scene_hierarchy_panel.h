#pragma once

#include <loom/core/core.h>
#include <loom/scene/scene.h>
#include <loom/scene/entity.h>

namespace Weaver {

    class SceneHierarchyPanel {
    public:
        SceneHierarchyPanel() = default;
        SceneHierarchyPanel(const std::shared_ptr<Loom::Scene>& context);

        void SetContext(const std::shared_ptr<Loom::Scene>& context);

        void OnImGuiRender();

    private:
        void DrawEntityNode(Loom::Entity entity);
        void DrawComponents(Loom::Entity entity);

    private:
        std::shared_ptr<Loom::Scene> mContext;
        Loom::Entity mSelectionContext;
    };

}