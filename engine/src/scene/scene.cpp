#include "loom/scene/scene.h"
#include "loom/core/uuid.h"
#include "loom/renderer/renderer_2d.h"
#include "loom/scene/components.h"
#include "loom/scene/entity.h"

namespace Loom {
    Scene::Scene() {}

    Scene::~Scene() {}

    template<typename Component>
    static void CopyComponent(entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& entt_map) {
        auto view = src.view<Component>();
        for (auto entity : view) {
            UUID         uuid          = src.get<IDComponent>(entity).ID;
            entt::entity dst_entity_id = entt_map.at(uuid);
            auto&        component     = src.get<Component>(entity);
            dst.emplace_or_replace<Component>(dst_entity_id, component);
        }
    }

    std::shared_ptr<Scene> Scene::Copy(std::shared_ptr<Scene> other) {
        std::shared_ptr<Scene>                 new_scene = std::make_shared<Scene>();
        std::unordered_map<UUID, entt::entity> entt_map;

        auto& src_registry = other->mRegistry;
        auto& dst_registry = new_scene->mRegistry;

        auto id_view = src_registry.view<IDComponent>();
        for (auto entity : id_view) {
            UUID        uuid       = src_registry.get<IDComponent>(entity).ID;
            const auto& name       = src_registry.get<TagComponent>(entity).Tag;
            Entity      new_entity = new_scene->CreateEntityWithUUID(uuid, name);
            entt_map[uuid]         = (entt::entity)new_entity;
        }

        CopyComponent<TransformComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<SpriteRendererComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<CameraComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<NativeScriptComponent>(dst_registry, src_registry, entt_map);

        return new_scene;
    }

    void Scene::OnViewportResize(uint32_t width, uint32_t height) {
        auto view = mRegistry.view<CameraComponent>();
        for (auto entity : view) {
            auto& camera_component = view.get<CameraComponent>(entity);
            if (!camera_component.FixedAspectRatio) {
                camera_component.Camera.SetViewportSize(width, height);
            }
        }
    }

    Entity Scene::CreateEntity(const std::string& name) {
        return CreateEntityWithUUID(UUID(), name);
    }

    Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name) {
        Entity entity = { mRegistry.create(), this };

        entity.AddComponent<IDComponent>().ID = uuid;
        entity.AddComponent<TransformComponent>();

        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag   = name.empty() ? "Entity" : name;

        return entity;
    }

    void Scene::DestroyEntity(Entity entity) {
        mRegistry.destroy(entity);
    }

    Entity Scene::GetEntityByUUID(UUID uuid) {
        auto view = mRegistry.view<IDComponent>();
        for (auto entity : view) {
            if (view.get<IDComponent>(entity).ID == uuid)
                return { entity, this };
        }
        return {};
    }

    void Scene::OnUpdate(Timestep ts) {
        mRegistry.view<NativeScriptComponent>().each([&](entt::entity entity_id, NativeScriptComponent& nsc) {
            if (!nsc.Instance) {
                nsc.Instance          = nsc.InstantiateScript();
                nsc.Instance->mEntity = Entity{ entity_id, this };
                nsc.Instance->OnCreate();
            }

            nsc.Instance->OnUpdate(ts);
        });

        Camera*   main_camera = nullptr;
        glm::mat4 camera_transform;

        auto view = mRegistry.view<TransformComponent, CameraComponent>();
        for (auto entity : view) {
            auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

            if (camera.Primary) {
                main_camera      = &camera.Camera;
                camera_transform = transform.GetTransform();
                break;
            }
        }

        if (main_camera) {
            Renderer2D::BeginScene(*main_camera, camera_transform);

            auto group = mRegistry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
            group.sort<TransformComponent>([](const auto& lhs, const auto& rhs) {
                return lhs.Translation.z < rhs.Translation.z;
            });
            for (auto entity : group) {
                auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

                int entity_id = (int)entt::to_entity(entity);

                Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color, entity_id);
            }

            Renderer2D::EndScene();
        }
    }

    void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera) {
        Renderer2D::BeginScene(camera);

        auto group = mRegistry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
        group.sort<TransformComponent>([](const auto& lhs, const auto& rhs) {
            return lhs.Translation.z < rhs.Translation.z;
        });
        for (auto entity : group) {
            auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
            if (sprite.Texture) {
                Renderer2D::DrawQuad(transform.GetTransform(), sprite.Texture, sprite.Color, (int)entt::to_entity(entity));
            } else {
                Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color, (int)entt::to_entity(entity));
            }
        }

        Renderer2D::EndScene();
    }

    void Scene::OnUpdateRuntime(Timestep ts) {
        // 1. Update Scripts
        // 2. Update Physics
        // 3. Find the primary camera
        mRegistry.view<NativeScriptComponent>().each([&](entt::entity entity_id, NativeScriptComponent& nsc) {
            if (!nsc.Instance) {
                nsc.Instance          = nsc.InstantiateScript();
                nsc.Instance->mEntity = Entity{ entity_id, this };
                nsc.Instance->OnCreate();
            }

            nsc.Instance->OnUpdate(ts);
        });

        Camera*   main_camera = nullptr;
        glm::mat4 camera_transform;

        auto view = mRegistry.view<TransformComponent, CameraComponent>();
        for (auto entity : view) {
            auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);
            if (camera.Primary) {
                main_camera      = &camera.Camera;
                camera_transform = transform.GetTransform();
                break;
            }
        }

        if (main_camera) {
            Renderer2D::BeginScene(*main_camera, camera_transform);

            auto group = mRegistry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
            group.sort<TransformComponent>([](const auto& lhs, const auto& rhs) {
                return lhs.Translation.z < rhs.Translation.z;
            });
            for (auto entity : group) {
                auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
                if (sprite.Texture) {
                    Renderer2D::DrawQuad(transform.GetTransform(), sprite.Texture, sprite.Color, (int)entt::to_entity(entity));
                } else {
                    Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color, (int)entt::to_entity(entity));
                }
            }

            Renderer2D::EndScene();
        }
    }

} // namespace Loom
