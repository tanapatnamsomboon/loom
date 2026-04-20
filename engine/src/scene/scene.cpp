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
        Camera* main_camera = nullptr;
        glm::mat4 camera_transform;

        auto view = mRegistry.view<TransformComponent, CameraComponent>();
        for (auto entity : view) {
            auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

            if (camera.Primary) {
                main_camera = &camera.Camera;
                camera_transform = transform.GetTransform();
                break;
            }
        }

        if (main_camera) {
            Renderer2D::BeginScene(*main_camera, camera_transform);

            auto group = mRegistry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
            for (auto entity : group) {
                auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
                Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color);
            }

            Renderer2D::EndScene();
        }
    }

} // namespace Loom