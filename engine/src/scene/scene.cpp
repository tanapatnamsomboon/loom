#include "loom/scene/scene.h"
#include "loom/core/uuid.h"
#include "loom/renderer/renderer_2d.h"
#include "loom/scene/components.h"
#include "loom/scene/entity.h"

namespace Loom {
    Scene::Scene() {
        mCameraIcon = Texture2D::Create("assets/icons/camera_icon.png");
    }

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

        auto nsc_view = src_registry.view<NativeScriptComponent>();
        for (auto entity : nsc_view) {
            UUID uuid = src_registry.get<IDComponent>(entity).ID;
            entt::entity dst_entity_id = entt_map.at(uuid);
            auto& src_nsc = src_registry.get<NativeScriptComponent>(entity);
            auto& dst_nsc = dst_registry.emplace_or_replace<NativeScriptComponent>(dst_entity_id, src_nsc);
            dst_nsc.Instance = nullptr;
        }

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

        mEntityMap[uuid] = (entt::entity)entity;

        return entity;
    }

    void Scene::DestroyEntity(Entity entity) {
        mEntityMap.erase(entity.GetComponent<IDComponent>().ID);
        mRegistry.destroy(entity);
    }

    Entity Scene::GetEntityByUUID(UUID uuid) {
        auto it = mEntityMap.find(uuid);
        if (it != mEntityMap.end())
            return { it->second, this };
        return {};
    }

    void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera, Entity selected_entity) {
        Renderer2D::BeginScene(camera);

        auto group = mRegistry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
        group.sort<TransformComponent>([](const auto& lhs, const auto& rhs) {
            return lhs.Translation.z < rhs.Translation.z;
        });
        for (auto entity : group) {
            auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
            if (sprite.Texture) {
                Renderer2D::DrawQuad(transform.GetTransform(), sprite.Texture, sprite.Color, sprite.TilingFactor, (int)entt::to_entity(entity));
            } else {
                Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color, (int)entt::to_entity(entity));
            }
        }

        auto camera_view = mRegistry.view<TransformComponent, CameraComponent>();
        for (auto entity : camera_view) {
            auto&     transform       = camera_view.get<TransformComponent>(entity);
            glm::mat4 camera_rotation = glm::mat4(glm::mat3(glm::transpose(camera.GetViewMatrix())));
            glm::mat4 billboard       = glm::translate(glm::mat4(1.0f), transform.Translation)
                                      * camera_rotation
                                      * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
            Renderer2D::DrawQuad(billboard, mCameraIcon, glm::vec4(1.0f), 1.0f, (int)entt::to_entity(entity));
        }

        if (selected_entity && selected_entity.HasComponent<CameraComponent>()) {
            DrawCameraFrustum(selected_entity.GetComponent<TransformComponent>(), selected_entity.GetComponent<CameraComponent>());
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
                    Renderer2D::DrawQuad(transform.GetTransform(), sprite.Texture, sprite.Color, sprite.TilingFactor, (int)entt::to_entity(entity));
                } else {
                    Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color, (int)entt::to_entity(entity));
                }
            }

            Renderer2D::EndScene();
        }
    }

    void Scene::DrawCameraFrustum(const TransformComponent& transform_component, const CameraComponent& camera_component) {
        const SceneCamera& camera    = camera_component.Camera;
        const glm::mat4&   world     = transform_component.GetTransform();
        const glm::vec4    color     = { 0.9f, 0.9f, 0.2f, 1.0f };
        const int          entity_id = -1;

        glm::vec3 near_corners[4];
        glm::vec3 far_corners[4];

        if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic) {
            float size = camera.GetOrthographicSize();
            float w    = size * camera.GetAspectRatio() * 0.5f;
            float h    = size * 0.5f;
            float n    = camera.GetOrthographicNearClip();
            float f    = camera.GetOrthographicFarClip();

            near_corners[0] = { -w, h, -n };
            near_corners[1] = { w, h, -n };
            near_corners[2] = { w, -h, -n };
            near_corners[3] = { -w, -h, -n };

            far_corners[0] = { -w, h, -f };
            far_corners[1] = { w, h, -f };
            far_corners[2] = { w, -h, -f };
            far_corners[3] = { -w, -h, -f };
        } else {
            float fov    = camera.GetPerspectiveVerticalFOV();
            float n      = camera.GetPerspectiveNearClip();
            float f      = camera.GetPerspectiveFarClip();
            float near_h = glm::tan(fov * 0.5f) * n;
            float near_w = near_h * camera.GetAspectRatio();
            float far_h  = glm::tan(fov * 0.5f) * f;
            float far_w  = far_h * camera.GetAspectRatio();

            near_corners[0] = { -near_w, near_h, -n };
            near_corners[1] = { near_w, near_h, -n };
            near_corners[2] = { near_w, -near_h, -n };
            near_corners[3] = { -near_w, -near_h, -n };

            far_corners[0] = { -far_w, far_h, -f };
            far_corners[1] = { far_w, far_h, -f };
            far_corners[2] = { far_w, -far_h, -f };
            far_corners[3] = { -far_w, -far_h, -f };
        }

        glm::vec3 wn[4], wf[4];
        for (int i = 0; i < 4; i++) {
            wn[i] = glm::vec3(world * glm::vec4(near_corners[i], 1.0f));
            wf[i] = glm::vec3(world * glm::vec4(far_corners[i], 1.0f));
        }

        Renderer2D::DrawLine(wn[0], wn[1], color, entity_id);
        Renderer2D::DrawLine(wn[1], wn[2], color, entity_id);
        Renderer2D::DrawLine(wn[2], wn[3], color, entity_id);
        Renderer2D::DrawLine(wn[3], wn[0], color, entity_id);

        Renderer2D::DrawLine(wf[0], wf[1], color, entity_id);
        Renderer2D::DrawLine(wf[1], wf[2], color, entity_id);
        Renderer2D::DrawLine(wf[2], wf[3], color, entity_id);
        Renderer2D::DrawLine(wf[3], wf[0], color, entity_id);

        Renderer2D::DrawLine(wn[0], wf[0], color, entity_id);
        Renderer2D::DrawLine(wn[1], wf[1], color, entity_id);
        Renderer2D::DrawLine(wn[2], wf[2], color, entity_id);
        Renderer2D::DrawLine(wn[3], wf[3], color, entity_id);
    }

} // namespace Loom
