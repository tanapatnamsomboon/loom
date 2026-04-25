#pragma once

#include <loom/core/core.h>
#include <loom/renderer/texture.h>
#include <loom/scene/entity.h>
#include <loom/scene/scene.h>

namespace Weaver {

    class SceneHierarchyPanel {
    public:
        SceneHierarchyPanel() = default;
        SceneHierarchyPanel(const std::shared_ptr<Loom::Scene>& context);

        void Init();

        void SetContext(const std::shared_ptr<Loom::Scene>& context);
        void SetSelectedEntity(const Loom::Entity& entity) { mSelectionContext = entity; }

        Loom::Entity GetSelectedEntity() const { return mSelectionContext; }

        void OnImGuiRender();

    private:
        void DrawEntityNode(Loom::Entity entity);
        void DrawComponents(Loom::Entity entity);

        std::shared_ptr<Loom::Texture2D> LoadTexture();

    private:
        std::shared_ptr<Loom::Scene> mContext;
        Loom::Entity                 mSelectionContext;

        std::shared_ptr<Loom::Texture2D> mCheckerboard;
    };

} // namespace Weaver
