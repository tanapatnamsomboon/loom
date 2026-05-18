#pragma once

#include "loom/core/core.h"
#include "loom/core/timestep.h"
#include "loom/core/uuid.h"
#include "loom/renderer/cubemap.h"
#include "loom/renderer/editor_camera.h"
#include "loom/renderer/texture.h"
#include <box2d/id.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

namespace JPH {
    class PhysicsSystem;
    class ContactListener;
}

namespace Loom {

    class Entity;

    struct TransformComponent;
    struct CameraComponent;
    struct Physics3DEventState; // Pimpl — body→entity map + thread-safe contact event queue (defined in scene.cpp)

    class LOOM_API Scene {
    public:
        Scene();
        ~Scene();

        static std::shared_ptr<Scene> Copy(std::shared_ptr<Scene> other);
        void                          OnViewportResize(uint32_t width, uint32_t height);

        void OnUpdateEditor(Timestep ts, EditorCamera& camera, Entity selected_entity);
        void OnRuntimeStart();
        void OnUpdateRuntime(Timestep ts);
        void OnRuntimeStop();

        Entity CreateEntity(const std::string& name = std::string());
        Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
        void   DestroyEntity(Entity entity);

        Entity    GetEntityByUUID(UUID uuid);
        Entity    GetEntityByTag(const std::string& tag);
        glm::mat4 GetWorldTransform(Entity entity);
        void      SetParent(Entity child, Entity parent);
        void      RemoveParent(Entity child);

        struct RaycastHit2D {
            bool         hit          = false;
            glm::vec2    point        = {};
            glm::vec2    normal       = {};
            entt::entity entityHandle = entt::null;
        };
        RaycastHit2D Raycast2D(glm::vec2 origin, glm::vec2 direction, float distance);

        std::vector<entt::entity> OverlapCircle2D(glm::vec2 center, float radius);
        std::vector<entt::entity> OverlapBox2D(glm::vec2 center, glm::vec2 half_extents);

        // 3D physics — operate via Rigidbody3DComponent::RuntimeBodyID. No-ops
        // when the scene isn't running, the entity has no rigidbody, or its body
        // hasn't been created. Forces are in Newtons; impulses in N*s.
        void      SetLinearVelocity3D(Entity entity, const glm::vec3& v);
        glm::vec3 GetLinearVelocity3D(Entity entity);
        void      ApplyForce3D       (Entity entity, const glm::vec3& f);
        void      ApplyImpulse3D     (Entity entity, const glm::vec3& j);

        template<typename... Components>
        auto GetAllEntitiesWith() {
            return mRegistry.view<Components...>();
        }

        void SetShowPhysicsColliders(bool show) { mShowPhysicsColliders = show; }
        bool IsShowingPhysicsColliders() const { return mShowPhysicsColliders; }

        // ── Skybox / IBL environment ──────────────────────────────────────
        // Path is project-relative (`environments/foo.hdr` etc.). Setting the
        // path triggers a lazy load + equirect→cubemap conversion on the next
        // GetSkyboxCubemap(). Empty path => no skybox; viewport falls back to
        // the framebuffer clear color, and IBL contributions use the neutral
        // grey fallback in mesh.frag.
        const std::string&              GetSkyboxPath()    const { return mSkyboxPath; }
        void                            SetSkyboxPath(const std::string& path);
        std::shared_ptr<TextureCubemap> GetSkyboxCubemap();

    private:
        void DrawCameraFrustum(const glm::mat4& world_transform, const CameraComponent& camera);
        void RenderPhysicsColliders();

        void OnPhysicsStart3D();
        void OnPhysicsStop3D();
        void DispatchPhysics3DEvents();

    private:
        entt::registry mRegistry;

        std::unordered_map<UUID, entt::entity> mEntityMap;
        std::shared_ptr<Texture2D> mCameraIcon;

        b2WorldId             mPhysicsWorld      = b2_nullWorldId;
        JPH::PhysicsSystem*   mPhysicsSystem3D   = nullptr;
        JPH::ContactListener* mContactListener3D = nullptr;
        std::unique_ptr<Physics3DEventState> mPhysics3DEvents;

        bool mShowPhysicsColliders = false;

        // Skybox / IBL state. `mSkyboxCubemap` is rebuilt lazily when `mSkyboxPath`
        // changes or first access happens after a scene load.
        std::string                     mSkyboxPath;
        std::shared_ptr<Texture2D>      mSkyboxEquirect;
        std::shared_ptr<TextureCubemap> mSkyboxCubemap;
        bool                            mSkyboxDirty = true;

        friend class Entity;
        friend class SceneHierarchyPanel;
        friend class SceneSerializer;
    };

} // namespace Loom
