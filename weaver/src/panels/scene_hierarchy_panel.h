#pragma once

#include <loom/core/core.h>
#include <loom/renderer/texture.h>
#include <loom/scene/entity.h>
#include <loom/scene/scene.h>
#include <functional>

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

        void SetSceneModifiedCallback(const std::function<void()>& callback) { mSceneModifiedCallback = callback; }
        void SetPlayMode(bool playing) { mIsPlayMode = playing; }

    private:
        void DrawEntityNode(Loom::Entity entity);
        void DrawComponents(Loom::Entity entity);

        std::shared_ptr<Loom::Texture2D> LoadTexture(const Loom::TextureSpecification& spec = {});

    private:
        std::shared_ptr<Loom::Scene> mContext;
        Loom::Entity                 mSelectionContext;

        std::shared_ptr<Loom::Texture2D> mCheckerboard;

        std::function<void()> mSceneModifiedCallback;
        bool                  mIsPlayMode = false;
    };

} // namespace Weaver
