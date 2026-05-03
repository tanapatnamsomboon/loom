#include "loom/scene/scene.h"
#include "loom/asset/asset_manager.h"
#include "loom/core/uuid.h"
#include "loom/project/project.h"
#include "loom/renderer/renderer_2d.h"
#include "loom/scene/components.h"
#include "loom/scene/entity.h"
#include "loom/scripting/scripting_engine.h"
#include <box2d/box2d.h>

namespace Loom {
    Scene::Scene() {
        std::string camera_icon_path = Project::GetEngineAssetFileSystemPath("icons/camera_icon.png").generic_string();
        mCameraIcon = AssetManager::GetTexture(camera_icon_path);
    }

    Scene::~Scene() {
        OnRuntimeStop();
    }

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

            if (!dst_nsc.ScriptName.empty()) {
                dst_nsc.BindByName(dst_nsc.ScriptName);
            }
        }

        CopyComponent<LuaScriptComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<Rigidbody2DComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<BoxCollider2DComponent>(dst_registry, src_registry, entt_map);

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

        RenderPhysicsColliders();

        Renderer2D::EndScene();
    }

    void Scene::OnRuntimeStart() {
        ScriptingEngine::OnRuntimeStart(this);

        b2WorldDef world_def = b2DefaultWorldDef();
        world_def.gravity = (b2Vec2){ 0.0f, -9.8f };
        mPhysicsWorld = b2CreateWorld(&world_def);

        auto view = mRegistry.view<Rigidbody2DComponent>();
        for (auto e : view) {
            Entity entity = { e, this };
            auto& transform = entity.GetComponent<TransformComponent>();
            auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

            b2BodyDef body_def = b2DefaultBodyDef();
            if (rb2d.Type == Rigidbody2DComponent::BodyType::Static) body_def.type = b2_staticBody;
            else if (rb2d.Type == Rigidbody2DComponent::BodyType::Dynamic) body_def.type = b2_dynamicBody;
            else body_def.type = b2_kinematicBody;

            body_def.position = { transform.Translation.x, transform.Translation.y };
            body_def.rotation = b2MakeRot(transform.Rotation.z);
            body_def.motionLocks.angularZ = rb2d.FixedRotation;

            rb2d.RuntimeBody = b2CreateBody(mPhysicsWorld, &body_def);

            if (entity.HasComponent<BoxCollider2DComponent>()) {
                auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();

                b2ShapeDef shape_def = b2DefaultShapeDef();
                shape_def.density = bc2d.Density;
                shape_def.material.friction = bc2d.Friction;
                shape_def.material.restitution = bc2d.Restitution;

                b2Polygon box = b2MakeOffsetBox(
                    bc2d.Size.x * transform.Scale.x,
                    bc2d.Size.y * transform.Scale.y,
                    { bc2d.Offset.x, bc2d.Offset.y },
                    b2MakeRot(0.0f)
                );

                bc2d.RuntimeFixture = b2CreatePolygonShape(rb2d.RuntimeBody, &shape_def, &box);
            }
        }
    }

    void Scene::OnUpdateRuntime(Timestep ts) {
        // 1. Update Scripts
        ScriptingEngine::OnRuntimeUpdate(ts, this);

        mRegistry.view<NativeScriptComponent>().each([&](entt::entity entity_id, NativeScriptComponent& nsc) {
            if (!nsc.IsValid())
                return;

            if (!nsc.Instance) {
                nsc.Instance = nsc.InstantiateScript();

                if (!nsc.Instance) {
                    LOOM_CORE_ERROR("Scene: failed to instantiate script: '{}' - skipping.", nsc.ScriptName);
                    return;
                }

                nsc.Instance->mEntity = Entity{ entity_id, this };
                nsc.Instance->OnCreate();
            }

            nsc.Instance->OnUpdate(ts);
        });

        // 2. Update Physics
        if (b2World_IsValid(mPhysicsWorld)) {
            int32_t sub_step_count = 4;
            b2World_Step(mPhysicsWorld, ts, sub_step_count);

            auto view = mRegistry.view<Rigidbody2DComponent>();
            for (auto e : view) {
                Entity entity = { e, this };
                auto& transform = entity.GetComponent<TransformComponent>();
                auto& rb2d      = entity.GetComponent<Rigidbody2DComponent>();

                b2Vec2 position = b2Body_GetPosition(rb2d.RuntimeBody);
                b2Rot  rotation = b2Body_GetRotation(rb2d.RuntimeBody);

                transform.Translation.x = position.x;
                transform.Translation.y = position.y;
                transform.Rotation.z = b2Rot_GetAngle(rotation);
            }
        }

        // 3. Find the primary camera
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

            RenderPhysicsColliders();

            Renderer2D::EndScene();
        }
    }

    void Scene::OnRuntimeStop() {
        ScriptingEngine::OnRuntimeStop();

        mRegistry.view<NativeScriptComponent>().each([](NativeScriptComponent& nsc) {
            if (nsc.Instance) {
                nsc.Instance->OnDestroy();
                nsc.DestroyScript(&nsc);
            }
        });

        if (b2World_IsValid(mPhysicsWorld)) {
            b2DestroyWorld(mPhysicsWorld);
            mPhysicsWorld = b2_nullWorldId;
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

    void Scene::RenderPhysicsColliders() {
        if (!mShowPhysicsColliders) return;

        auto view = mRegistry.view<TransformComponent, BoxCollider2DComponent>();
        for (auto entity : view) {
            auto [transform, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(entity);

            glm::vec3 translation = transform.Translation + glm::vec3(bc2d.Offset, 0.001f);
            glm::vec3 scale = transform.Scale * glm::vec3(bc2d.Size * 2.0f, 1.0f);

            glm::mat4 transform_mat = glm::translate(glm::mat4(1.0f), translation)
                                    * glm::rotate(glm::mat4(1.0f), transform.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
                                    * glm::scale(glm::mat4(1.0f), scale);

            glm::vec3 p0 = transform_mat * glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f);
            glm::vec3 p1 = transform_mat * glm::vec4( 0.5f, -0.5f, 0.0f, 1.0f);
            glm::vec3 p2 = transform_mat * glm::vec4( 0.5f,  0.5f, 0.0f, 1.0f);
            glm::vec3 p3 = transform_mat * glm::vec4(-0.5f,  0.5f, 0.0f, 1.0f);

            glm::vec4 color = { 0.1f, 0.9f, 0.1f, 1.0f };
            int entity_id = (int)entt::to_entity(entity);

            Renderer2D::DrawLine(p0, p1, color, entity_id);
            Renderer2D::DrawLine(p1, p2, color, entity_id);
            Renderer2D::DrawLine(p2, p3, color, entity_id);
            Renderer2D::DrawLine(p3, p0, color, entity_id);
        }
    }

} // namespace Loom
