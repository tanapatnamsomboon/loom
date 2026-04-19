#include "loom/scene/scene.h"
#include "loom/scene/components.h"
#include "loom/scene/entity.h"
#include "loom/renderer/renderer_2d.h"

namespace Loom {

    Scene::Scene() {}

    Scene::~Scene() {}

    Entity Scene::CreateEntity(const std::string& name) {
        Entity entity = { mRegistry.create(), this };

        entity.AddComponent<TransformComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag = name.empty() ? "Entity" : name;

        return entity;
    }

    void Scene::OnUpdate(Timestep ts) {
        auto group = mRegistry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
        for (auto entity : group) {
            auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
            Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color);
        }
    }

} // namespace Loom