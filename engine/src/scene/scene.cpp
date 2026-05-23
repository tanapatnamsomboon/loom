#include "loom/scene/scene.h"
#include "loom/asset/asset_manager.h"
#include "loom/audio/audio_engine.h"
#include "loom/core/uuid.h"
#include "loom/physics/physics_engine_3d.h"
#include "loom/project/project.h"
#include "loom/renderer/renderer_2d.h"
#include "loom/renderer/renderer_3d.h"
#include "loom/scene/components.h"
#include "loom/scene/entity.h"
#include "loom/scripting/scripting_engine.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <random>
#include <unordered_set>
#include <box2d/box2d.h>
#include <glm/gtc/quaternion.hpp>

// Jolt — only the bits we touch in this TU.
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/EActivation.h>
#include <mutex>

namespace Loom {
    Scene::Scene() {
        std::string camera_icon_path = Project::GetEngineAssetFileSystemPath("icons/camera_icon.png").generic_string();
        mCameraIcon = AssetManager::GetTexture(camera_icon_path);
        mPhysics3DEvents = std::make_unique<Physics3DEventState>();
    }

    Scene::~Scene() {
        OnRuntimeStop();
    }

    void Scene::SetSkyboxPath(const std::string& path) {
        if (mSkyboxPath == path) return;
        mSkyboxPath    = path;
        mSkyboxEquirect.reset();
        mSkyboxCubemap.reset();
        mIrradianceCubemap.reset();
        mPrefilterCubemap.reset();
        mSkyboxDirty   = true;
    }

    std::shared_ptr<TextureCubemap> Scene::GetSkyboxCubemap() {
        if (!mSkyboxDirty) return mSkyboxCubemap;
        mSkyboxDirty = false;

        // Empty path = scene has no environment assigned. Returns null; the
        // editor layers its own fallback HDR on top for build-time UX, play
        // mode honours the scene's empty config and renders no skybox.
        if (mSkyboxPath.empty()) {
            mSkyboxEquirect.reset();
            mSkyboxCubemap.reset();
            return nullptr;
        }

        std::string abs_path = Project::GetAssetFileSystemPath(mSkyboxPath).generic_string();

        // Lazy load: HDR equirect → 6-face cubemap. Conversion happens via a
        // one-time GPU pass in TextureCubemap::CreateFromEquirect.
        mSkyboxEquirect = AssetManager::GetTexture(abs_path);
        if (!mSkyboxEquirect) {
            LOOM_CORE_WARN("Skybox HDR failed to load: {}", abs_path);
            mSkyboxCubemap.reset();
            return nullptr;
        }
        // 2048 per face: the editor camera uses a 30° FOV, so viewport density
        // (≈22 px/deg at 660 vp-px tall) is ~2× the cubemap's angular density
        // at 1024². That mismatch causes a visible bilinear upscale ("looks like
        // 480p"). 2048² brings cubemap density to ~22.8 px/deg — near 1:1 with
        // the viewport at typical editor sizes — so cubemap→viewport sampling
        // doesn't introduce additional softening. Does NOT add detail beyond
        // what a 4K equirect carries (the HDR is still the information ceiling);
        // it just removes the upsample-blur step. VRAM cost: ~200 MB RGB16F
        // with full mip chain.
        mSkyboxCubemap = TextureCubemap::CreateFromEquirect(mSkyboxEquirect, 2048);
        return mSkyboxCubemap;
    }

    std::shared_ptr<TextureCubemap> Scene::GetIrradianceCubemap() {
        if (mIrradianceCubemap) return mIrradianceCubemap;
        // Trigger skybox load if it hasn't happened. Irradiance derives from
        // the env cubemap; both share the same dirty flag.
        auto env = GetSkyboxCubemap();
        if (!env) return nullptr;
        // 128² instead of the textbook 32². Diffuse irradiance is low-freq,
        // but a sphere wrapping the cubemap sees ~face_size × 4 texels along
        // any great circle — at 32² that's only ~128 discrete samples per
        // sphere equator, where bilinear interpolation leaves a visibly
        // stepped gradient. 128² gives ~512 texels per great circle, enough
        // that interpolation is imperceptible. Cost: 128×128×6×6 ≈ 600 KB.
        mIrradianceCubemap = TextureCubemap::CreateIrradiance(env, 128);
        return mIrradianceCubemap;
    }

    std::shared_ptr<TextureCubemap> Scene::GetPrefilterCubemap() {
        if (mPrefilterCubemap) return mPrefilterCubemap;
        auto env = GetSkyboxCubemap();
        if (!env) return nullptr;
        // 256² with a full mip chain (mips 0..8). Roughness 0 = mip 0 (mirror,
        // matches env resolution), roughness 1 = mip 8 (4² fully-rough). Cost:
        // ~525 KB RGB16F with the mip chain. The build is the most expensive
        // step in the IBL pipeline (1024 samples × 6 faces × Σ mip texels),
        // takes ~half a second on a mid-range GPU — runs once per scene load.
        mPrefilterCubemap = TextureCubemap::CreatePrefiltered(env, 256);
        return mPrefilterCubemap;
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

    // Copy every listed component that exists on `src` onto `dst`, within one
    // registry. Component copy-constructors null out runtime handles (physics
    // bodies, sounds, GPU resources) — the same contract Scene::Copy relies on.
    template<typename... Component>
    static void CopyEntityComponents(entt::registry& reg, entt::entity dst, entt::entity src) {
        ([&] {
            if (reg.all_of<Component>(src))
                reg.emplace_or_replace<Component>(dst, reg.get<Component>(src));
        }(), ...);
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
        CopyComponent<MeshRendererComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<AnimationComponent>(dst_registry, src_registry, entt_map);
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
        CopyComponent<TilemapComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<TextComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<AudioSourceComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<ParticleComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<Rigidbody2DComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<BoxCollider2DComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<CircleCollider2DComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<DirectionalLightComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<PointLightComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<Rigidbody3DComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<BoxCollider3DComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<SphereCollider3DComponent>(dst_registry, src_registry, entt_map);
        CopyComponent<CapsuleCollider3DComponent>(dst_registry, src_registry, entt_map);

        // Copy relationship structure, remapping entt handles through the UUID map
        auto rel_view = src_registry.view<RelationshipComponent>();
        for (auto src_entity : rel_view) {
            UUID         uuid          = src_registry.get<IDComponent>(src_entity).ID;
            entt::entity dst_entity_id = entt_map.at(uuid);
            auto&        src_rel       = src_registry.get<RelationshipComponent>(src_entity);

            RelationshipComponent dst_rel;
            if (src_rel.Parent != entt::null) {
                UUID parent_uuid  = src_registry.get<IDComponent>(src_rel.Parent).ID;
                dst_rel.Parent    = entt_map.at(parent_uuid);
            }
            for (auto child : src_rel.Children) {
                UUID child_uuid = src_registry.get<IDComponent>(child).ID;
                dst_rel.Children.push_back(entt_map.at(child_uuid));
            }
            dst_registry.emplace_or_replace<RelationshipComponent>(dst_entity_id, dst_rel);
        }

        // Environment carries over to the play-mode copy. Skipping this leaves
        // the runtime scene with no skybox + zero IBL even when the source
        // scene had an HDR assigned — surfaces as "skybox disappears when I
        // hit Play". Sharing the prebuilt cubemap pointers also avoids
        // rebuilding the (~200 MB) env + irradiance pair on every play start.
        new_scene->mSkyboxPath        = other->mSkyboxPath;
        new_scene->mSkyboxEquirect    = other->mSkyboxEquirect;
        new_scene->mSkyboxCubemap     = other->mSkyboxCubemap;
        new_scene->mIrradianceCubemap = other->mIrradianceCubemap;
        new_scene->mPrefilterCubemap  = other->mPrefilterCubemap;
        new_scene->mSkyboxDirty       = other->mSkyboxDirty;

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

    // Physics3D contact-event plumbing — the Pimpl payload declared in scene.h.
    // Defined here, near the top, so Scene members above (DestroyEntity) can
    // touch its fields; LoomContactListener3D further down also uses it.
    struct Physics3DEventState {
        enum class Kind : uint8_t { Begin, End };
        struct Event { Kind kind; uint32_t body_a; uint32_t body_b; };

        std::vector<Event>                         events;       // drained on main thread after Update()
        std::mutex                                 events_mutex; // events[] is written from Jolt worker threads
        std::unordered_map<uint32_t, entt::entity> body_to_entity;
    };

    void Scene::DestroyEntity(Entity entity) {
        // Recursively destroy children first (copy list — destroying modifies it)
        if (entity.HasComponent<RelationshipComponent>()) {
            auto& rel          = entity.GetComponent<RelationshipComponent>();
            auto  children_copy = rel.Children;
            for (auto child_handle : children_copy) {
                Entity child = { child_handle, this };
                if (child)
                    DestroyEntity(child);
            }
            // Unlink from parent
            if (rel.Parent != entt::null) {
                Entity parent_entity = { rel.Parent, this };
                if (parent_entity && parent_entity.HasComponent<RelationshipComponent>()) {
                    auto& parent_rel = parent_entity.GetComponent<RelationshipComponent>();
                    auto  it         = std::find(parent_rel.Children.begin(), parent_rel.Children.end(), (entt::entity)entity);
                    if (it != parent_rel.Children.end())
                        parent_rel.Children.erase(it);
                    if (parent_rel.Children.empty() && parent_rel.Parent == entt::null)
                        parent_entity.RemoveComponent<RelationshipComponent>();
                }
            }
        }

        // Tear down any runtime Box2D body so destroying an entity mid-play
        // can't leave an orphaned body still emitting contact/sensor events or
        // being hit by raycasts (which would resolve to this dead entity).
        if (entity.HasComponent<Rigidbody2DComponent>()) {
            auto& rb = entity.GetComponent<Rigidbody2DComponent>();
            if (b2Body_IsValid(rb.RuntimeBody)) {
                b2DestroyBody(rb.RuntimeBody);
                rb.RuntimeBody = b2_nullBodyId;
            }
        }
        if (entity.HasComponent<TilemapComponent>()) {
            auto& tc = entity.GetComponent<TilemapComponent>();
            if (b2Body_IsValid(tc.RuntimeBody)) {
                b2DestroyBody(tc.RuntimeBody);
                tc.RuntimeBody = b2_nullBodyId;
            }
        }
        if (entity.HasComponent<Rigidbody3DComponent>()) {
            auto& rb = entity.GetComponent<Rigidbody3DComponent>();
            if (mPhysicsSystem3D && rb.RuntimeBodyID != 0xffffffffu) {
                JPH::BodyID         id(rb.RuntimeBodyID);
                JPH::BodyInterface& bi = mPhysicsSystem3D->GetBodyInterface();
                bi.RemoveBody(id);
                bi.DestroyBody(id);
                mPhysics3DEvents->body_to_entity.erase(rb.RuntimeBodyID);
                rb.RuntimeBodyID = 0xffffffffu;
            }
        }

        mEntityMap.erase(entity.GetComponent<IDComponent>().ID);
        mRegistry.destroy(entity);
    }

    Entity Scene::DuplicateEntity(Entity src) {
        if (!src) return {};

        std::string name = src.HasComponent<TagComponent>()
            ? src.GetComponent<TagComponent>().Tag : std::string{};
        Entity dst = CreateEntity(name);

        CopyEntityComponents<
            TransformComponent, SpriteRendererComponent, MeshRendererComponent,
            AnimationComponent, CameraComponent, LuaScriptComponent,
            TilemapComponent, TextComponent, AudioSourceComponent,
            ParticleComponent, Rigidbody2DComponent, BoxCollider2DComponent,
            CircleCollider2DComponent, DirectionalLightComponent,
            PointLightComponent, Rigidbody3DComponent, BoxCollider3DComponent,
            SphereCollider3DComponent, CapsuleCollider3DComponent
        >(mRegistry, (entt::entity)dst, (entt::entity)src);

        // Native scripts: copy the binding, never alias the live instance.
        if (src.HasComponent<NativeScriptComponent>()) {
            auto& dst_nsc = dst.AddComponent<NativeScriptComponent>(
                src.GetComponent<NativeScriptComponent>());
            dst_nsc.Instance = nullptr;
            if (!dst_nsc.ScriptName.empty())
                dst_nsc.BindByName(dst_nsc.ScriptName);
        }

        // Recurse into the child subtree, parenting each copy under dst.
        for (Entity child : src.GetChildren()) {
            Entity dup_child = DuplicateEntity(child);
            if (dup_child) SetParent(dup_child, dst);
        }

        // Place the copy as a sibling of the source (SetParent reparents
        // cleanly, so a child copy's transient parent is harmless).
        if (Entity parent = src.GetParent())
            SetParent(dst, parent);

        return dst;
    }

    glm::mat4 Scene::GetWorldTransform(Entity entity) {
        glm::mat4 local = entity.GetComponent<TransformComponent>().GetTransform();
        if (!entity.HasComponent<RelationshipComponent>())
            return local;
        entt::entity parent_handle = entity.GetComponent<RelationshipComponent>().Parent;
        if (parent_handle == entt::null)
            return local;
        Entity parent = { parent_handle, this };
        if (!parent)
            return local;
        return GetWorldTransform(parent) * local;
    }

    void Scene::SetParent(Entity child, Entity parent) {
        RemoveParent(child);

        if (!child.HasComponent<RelationshipComponent>())
            child.AddComponent<RelationshipComponent>();
        child.GetComponent<RelationshipComponent>().Parent = (entt::entity)parent;

        if (!parent.HasComponent<RelationshipComponent>())
            parent.AddComponent<RelationshipComponent>();
        parent.GetComponent<RelationshipComponent>().Children.push_back((entt::entity)child);
    }

    void Scene::RemoveParent(Entity child) {
        if (!child.HasComponent<RelationshipComponent>())
            return;
        auto& child_rel = child.GetComponent<RelationshipComponent>();
        if (child_rel.Parent == entt::null)
            return;

        Entity parent_entity = { child_rel.Parent, this };
        if (parent_entity && parent_entity.HasComponent<RelationshipComponent>()) {
            auto& parent_rel = parent_entity.GetComponent<RelationshipComponent>();
            auto  it         = std::find(parent_rel.Children.begin(), parent_rel.Children.end(), (entt::entity)child);
            if (it != parent_rel.Children.end())
                parent_rel.Children.erase(it);
            if (parent_rel.Children.empty() && parent_rel.Parent == entt::null)
                parent_entity.RemoveComponent<RelationshipComponent>();
        }

        child_rel.Parent = entt::null;
        if (child_rel.Children.empty())
            child.RemoveComponent<RelationshipComponent>();
    }

    Entity Scene::GetEntityByUUID(UUID uuid) {
        auto it = mEntityMap.find(uuid);
        if (it != mEntityMap.end())
            return { it->second, this };
        return {};
    }

    Entity Scene::GetEntityByTag(const std::string& tag) {
        auto view = mRegistry.view<TagComponent>();
        for (auto e : view) {
            if (view.get<TagComponent>(e).Tag == tag)
                return { e, this };
        }
        return {};
    }

    static void DrawSprite(entt::registry& registry, entt::entity e, const glm::mat4& world, const SpriteRendererComponent& sprite) {
        if (registry.all_of<AnimationComponent>(e)) {
            const auto& anim = registry.get<AnimationComponent>(e);
            const AnimationClip* clip = anim.GetCurrentClip();
            if (clip && !clip->Frames.empty() && sprite.Texture) {
                int frame_idx = std::clamp(anim.CurrentFrame, 0, (int)clip->Frames.size() - 1);
                const auto& uv = clip->Frames[frame_idx];
                const glm::vec2 tex_coords[4] = {
                    { uv.x, uv.y }, { uv.z, uv.y }, { uv.z, uv.w }, { uv.x, uv.w }
                };
                Renderer2D::DrawQuad(world, sprite.Texture, tex_coords, sprite.Color, (int)(uint32_t)e);
                return;
            }
        }
        if (sprite.Texture)
            Renderer2D::DrawQuad(world, sprite.Texture, sprite.Color, sprite.TilingFactor, (int)(uint32_t)e);
        else
            Renderer2D::DrawQuad(world, sprite.Color, (int)(uint32_t)e);
    }

    static float ParticleRandom01() {
        thread_local std::mt19937 rng{ std::random_device{}() };
        thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng);
    }

    static float ParticleRandomRange(float min_v, float max_v) {
        return min_v + (max_v - min_v) * ParticleRandom01();
    }

    static glm::vec2 ParticleSampleShape(const ParticleComponent& pc) {
        switch (pc.Shape) {
            case ParticleComponent::EmitterShape::Box: {
                float x = ParticleRandomRange(-pc.ShapeSize.x, pc.ShapeSize.x);
                float y = ParticleRandomRange(-pc.ShapeSize.y, pc.ShapeSize.y);
                return { x, y };
            }
            case ParticleComponent::EmitterShape::Circle: {
                // sqrt for uniform area distribution
                float r     = pc.ShapeSize.x * std::sqrt(ParticleRandom01());
                float theta = ParticleRandom01() * 6.2831853f;
                return { r * std::cos(theta), r * std::sin(theta) };
            }
            case ParticleComponent::EmitterShape::Point:
            default:
                return { 0.0f, 0.0f };
        }
    }

    static void TickParticles(ParticleComponent& pc, const glm::mat4& world, float ts) {
        glm::vec3 emitter_world_pos = glm::vec3(world[3]);

        // 1. Spawn new particles
        if (pc.Emitting && pc.SpawnRate > 0.0f) {
            pc.SpawnAccumulator += ts * pc.SpawnRate;
            int to_spawn = (int)pc.SpawnAccumulator;
            pc.SpawnAccumulator -= (float)to_spawn;

            int cap = std::max(0, pc.MaxParticles);
            int free_slots = cap - (int)pc.Live.size();
            to_spawn = std::clamp(to_spawn, 0, free_slots);

            for (int i = 0; i < to_spawn; ++i) {
                ParticleComponent::ParticleInstance p;
                glm::vec2 spawn_offset = ParticleSampleShape(pc);
                if (pc.Space == ParticleComponent::SimulationSpace::World)
                    p.Position = glm::vec2(emitter_world_pos) + spawn_offset;
                else
                    p.Position = spawn_offset;
                p.Velocity = { ParticleRandomRange(pc.VelocityMin.x, pc.VelocityMax.x),
                               ParticleRandomRange(pc.VelocityMin.y, pc.VelocityMax.y) };
                p.Rotation = 0.0f;
                p.Age      = 0.0f;
                p.Lifetime = std::max(0.0001f, ParticleRandomRange(pc.LifetimeMin, pc.LifetimeMax));
                pc.Live.push_back(p);
            }
        } else {
            pc.SpawnAccumulator = 0.0f;
        }

        // 2. Integrate motion + age, removing expired in-place
        glm::vec2 gravity_step = pc.Gravity * pc.GravityScale * ts;
        size_t write = 0;
        for (size_t read = 0; read < pc.Live.size(); ++read) {
            auto& p = pc.Live[read];
            p.Age += ts;
            if (p.Age >= p.Lifetime)
                continue;
            p.Velocity += gravity_step;
            p.Position += p.Velocity * ts;
            p.Rotation += pc.RotationSpeed * ts;
            if (write != read)
                pc.Live[write] = p;
            ++write;
        }
        pc.Live.resize(write);
    }

    static void DrawParticles(ParticleComponent& pc, const glm::mat4& world, int entity_id) {
        if (pc.Live.empty()) return;

        glm::vec3 emitter_world_pos = glm::vec3(world[3]);
        bool      is_local          = pc.Space == ParticleComponent::SimulationSpace::Local;

        for (const auto& p : pc.Live) {
            float t = std::clamp(p.Age / p.Lifetime, 0.0f, 1.0f);
            glm::vec4 color = glm::mix(pc.ColorBegin, pc.ColorEnd, t);
            float     size  = glm::mix(pc.SizeBegin,  pc.SizeEnd,  t);
            if (size <= 0.0f) continue;

            glm::mat4 local = glm::translate(glm::mat4(1.0f), { p.Position.x, p.Position.y, 0.0f })
                            * glm::rotate(glm::mat4(1.0f), p.Rotation, { 0.0f, 0.0f, 1.0f })
                            * glm::scale(glm::mat4(1.0f), { size, size, 1.0f });

            glm::mat4 transform = is_local
                ? world * local
                // World mode: ignore emitter rotation/scale; keep particles flat at emitter Z.
                : glm::translate(glm::mat4(1.0f), { p.Position.x, p.Position.y, emitter_world_pos.z })
                    * glm::rotate(glm::mat4(1.0f), p.Rotation, { 0.0f, 0.0f, 1.0f })
                    * glm::scale(glm::mat4(1.0f), { size, size, 1.0f });

            if (pc.Texture)
                Renderer2D::DrawQuad(transform, pc.Texture, color, 1.0f, entity_id);
            else
                Renderer2D::DrawQuad(transform, color, entity_id);
        }
    }

    static void UpdateAndDrawParticleEntity(Scene* scene, entt::registry& registry,
                                            entt::entity e, ParticleComponent& pc, float ts) {
        if (!pc.TexturePath.empty()) {
            std::string abs_path = Project::GetAssetFileSystemPath(pc.TexturePath).generic_string();
            if (!pc.Texture || pc.Texture->GetPath() != abs_path)
                pc.Texture = AssetManager::GetTexture(abs_path);
        } else {
            pc.Texture = nullptr;
        }

        glm::mat4 world = scene->GetWorldTransform({ e, scene });
        TickParticles(pc, world, ts);
        DrawParticles(pc, world, (int)(uint32_t)e);
    }

    static void GatherAndUploadLights(Scene* scene, entt::registry& registry) {
        std::vector<Renderer3D::DirectionalLight> dirs;
        std::vector<Renderer3D::PointLight>       points;

        dirs.reserve(Renderer3D::kMaxDirectionalLights);
        points.reserve(Renderer3D::kMaxPointLights);

        for (auto e : registry.view<TransformComponent, DirectionalLightComponent>()) {
            const auto& dl = registry.get<DirectionalLightComponent>(e);
            glm::mat4 world = scene->GetWorldTransform({ e, scene });
            // Light's forward direction is local -Z transformed by rotation only
            // (w=0 ignores the translation column).
            glm::vec3 dir = glm::normalize(glm::vec3(world * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
            dirs.push_back({ dir, dl.Color * dl.Intensity });
            if ((int)dirs.size() >= Renderer3D::kMaxDirectionalLights) break;
        }

        for (auto e : registry.view<TransformComponent, PointLightComponent>()) {
            const auto& pl = registry.get<PointLightComponent>(e);
            glm::mat4 world = scene->GetWorldTransform({ e, scene });
            glm::vec3 pos = glm::vec3(world[3]);
            points.push_back({ pos, pl.Color * pl.Intensity, pl.Range });
            if ((int)points.size() >= Renderer3D::kMaxPointLights) break;
        }

        Renderer3D::SetLights(
            dirs.empty()   ? nullptr : dirs.data(),   (int)dirs.size(),
            points.empty() ? nullptr : points.data(), (int)points.size()
        );
    }

    // Max world-space distance from the camera that is covered by the cascaded
    // shadow maps. Cascades partition [cam_near, kShadowMaxDistance] so the
    // foreground stays crisp while distant geometry still receives shadow.
    static constexpr float kShadowMaxDistance = 200.0f;
    // Lambda blends uniform vs. logarithmic cascade splits: 0 = uniform (equal
    // world distance per cascade), 1 = logarithmic (geometric ratio). 0.5 is
    // the practical default — close cascades stay small for foreground detail
    // while far cascades cover a lot of range.
    static constexpr float kCascadeSplitLambda = 0.5f;

    // Returns the 8 world-space corners of the camera frustum slice defined by
    // [near_dist, far_dist] in **world-space distance** along the view direction.
    // Designed up-front to support cascade splits later (Slice D) — a single
    // cascade just passes [near, kShadowMaxDistance].
    static std::array<glm::vec3, 8> GetCameraFrustumCornersWS(const glm::mat4& cam_view,
                                                              const glm::mat4& cam_proj,
                                                              float near_dist,
                                                              float far_dist) {
        // Recover fov & aspect from the projection matrix:
        //   proj[1][1] = 1 / tan(fov_y / 2)
        //   proj[0][0] = proj[1][1] / aspect
        float tan_half_fov_y = 1.0f / cam_proj[1][1];
        float tan_half_fov_x = 1.0f / cam_proj[0][0];

        glm::mat4 inv_view = glm::inverse(cam_view);
        std::array<glm::vec3, 8> corners;
        int i = 0;
        for (float d : { near_dist, far_dist }) {
            float h = d * tan_half_fov_x;
            float v = d * tan_half_fov_y;
            for (float x : { -h, h }) {
                for (float y : { -v, v }) {
                    // View space is right-handed with -Z forward.
                    glm::vec4 corner_ws = inv_view * glm::vec4(x, y, -d, 1.0f);
                    corners[i++] = glm::vec3(corner_ws);
                }
            }
        }
        return corners;
    }

    // Builds a sphere-bounded, texel-snapped orthographic shadow VP for a single
    // cascade slice [slice_near, slice_far] of the camera's view frustum, lit
    // from `dir`. Sphere bounding gives rotation-invariance (stable ortho size
    // as camera spins); texel snap keeps the shadow grid stationary in world
    // space (no shimmer).
    static glm::mat4 BuildShadowLightVPForSlice(const glm::mat4& cam_view, const glm::mat4& cam_proj,
                                                float slice_near, float slice_far,
                                                const glm::vec3& dir) {
        std::array<glm::vec3, 8> corners = GetCameraFrustumCornersWS(
            cam_view, cam_proj, slice_near, slice_far);

        glm::vec3 center(0.0f);
        for (const auto& c : corners) center += c;
        center /= 8.0f;

        float radius = 0.0f;
        for (const auto& c : corners) {
            radius = std::max(radius, glm::length(c - center));
        }

        // Light view basis. glm::lookAt's up degenerates when parallel to dir.
        glm::vec3 up = (std::abs(dir.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);

        // Texel snap against the light's world-space X/Y axes directly (going
        // through view-space would be a no-op since center is the lookAt target).
        glm::vec3 light_z_world = -dir;
        glm::vec3 light_x_world = glm::normalize(glm::cross(up, light_z_world));
        glm::vec3 light_y_world = glm::cross(light_z_world, light_x_world);

        float texel_size = 2.0f * radius / (float)Renderer3D::kShadowMapSize;
        float cx = glm::dot(center, light_x_world);
        float cy = glm::dot(center, light_y_world);
        float cz = glm::dot(center, light_z_world);
        cx = std::floor(cx / texel_size) * texel_size;
        cy = std::floor(cy / texel_size) * texel_size;
        glm::vec3 snapped_center = light_x_world * cx + light_y_world * cy + light_z_world * cz;

        glm::mat4 light_view = glm::lookAt(snapped_center - dir * radius, snapped_center, up);

        // Pull the near plane toward the light so casters between the light and
        // the visible region still record into the shadow map.
        const float kCasterPullBack = 50.0f;
        glm::mat4 light_proj = glm::ortho(-radius, radius,
                                          -radius, radius,
                                          -kCasterPullBack, 2.0f * radius);
        return light_proj * light_view;
    }

    // Computes cascade split distances using the "practical split scheme"
    // (Engel '08): a blend of uniform and logarithmic splits. Close cascades
    // get small ranges (high detail), far cascades cover lots of distance.
    static void ComputeCascadeSplits(float cam_near, float cam_far,
                                     float lambda, float out_splits[Renderer3D::kCascadeCount]) {
        for (int i = 0; i < Renderer3D::kCascadeCount; ++i) {
            float p   = (float)(i + 1) / (float)Renderer3D::kCascadeCount;
            float log = cam_near * std::pow(cam_far / cam_near, p);
            float uni = cam_near + (cam_far - cam_near) * p;
            out_splits[i] = lambda * log + (1.0f - lambda) * uni;
        }
    }

    // Depth-only multi-cascade pass that fills the cascade shadow maps. Skips
    // if there is no directional light — the next mesh draw runs shadow-less.
    static void RunShadowPass(Scene* scene, entt::registry& registry,
                              const glm::mat4& cam_view, const glm::mat4& cam_proj) {
        // Need a directional light to build the light VP.
        glm::vec3 dir;
        bool      have_light = false;
        for (auto e : registry.view<TransformComponent, DirectionalLightComponent>()) {
            glm::mat4 world = scene->GetWorldTransform({ e, scene });
            dir = glm::normalize(glm::vec3(world * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
            have_light = true;
            break;
        }
        if (!have_light) return;

        // Camera near recovered from the projection matrix (see earlier notes).
        float cam_near = cam_proj[3][2] / (cam_proj[2][2] - 1.0f);

        // Compute per-cascade ranges (far distances) and matching VPs.
        float splits[Renderer3D::kCascadeCount];
        ComputeCascadeSplits(cam_near, kShadowMaxDistance, kCascadeSplitLambda, splits);
        Renderer3D::SetCascadeSplits(splits);

        // Mesh entities are walked once per cascade. Lazy-loading the mesh here
        // mirrors DrawMeshEntity so the shadow pass picks up freshly-assigned meshes.
        auto mesh_view = registry.view<TransformComponent, MeshRendererComponent>();

        float prev_split = cam_near;
        for (int c = 0; c < Renderer3D::kCascadeCount; ++c) {
            float curr_split = splits[c];
            glm::mat4 light_vp = BuildShadowLightVPForSlice(cam_view, cam_proj,
                                                            prev_split, curr_split, dir);
            prev_split = curr_split;

            Renderer3D::BeginShadowPass(c, light_vp);
            for (auto e : mesh_view) {
                auto& mrc = registry.get<MeshRendererComponent>(e);
                if (!mrc.MeshPath.empty()) {
                    std::string abs_path = Project::GetAssetFileSystemPath(mrc.MeshPath).generic_string();
                    if (!mrc.Mesh || mrc.Mesh->GetPath() != abs_path)
                        mrc.Mesh = AssetManager::GetMesh(abs_path);
                }
                if (!mrc.Mesh) continue;
                glm::mat4 world = scene->GetWorldTransform({ e, scene });
                Renderer3D::SubmitShadow(mrc.Mesh, world);
            }
            Renderer3D::EndShadowPass();
        }
    }

    static void DrawMeshEntity(Scene* scene, entt::registry& /*registry*/, entt::entity e, MeshRendererComponent& mrc) {
        // Lazy-load mesh
        if (!mrc.MeshPath.empty()) {
            std::string abs_path = Project::GetAssetFileSystemPath(mrc.MeshPath).generic_string();
            if (!mrc.Mesh || mrc.Mesh->GetPath() != abs_path)
                mrc.Mesh = AssetManager::GetMesh(abs_path);
        }
        if (!mrc.Mesh) return;

        // Lazy-load albedo texture
        if (!mrc.AlbedoTexturePath.empty()) {
            std::string abs_tex = Project::GetAssetFileSystemPath(mrc.AlbedoTexturePath).generic_string();
            if (!mrc.AlbedoTexture || mrc.AlbedoTexture->GetPath() != abs_tex)
                mrc.AlbedoTexture = AssetManager::GetTexture(abs_tex, kMeshAlbedoTextureSpec);
        }

        // Lazy-load ORM + emissive textures. Both use the mesh-albedo texture
        // spec (Linear + mips); the shader interprets channels by convention.
        if (!mrc.ORMTexturePath.empty()) {
            std::string abs_orm = Project::GetAssetFileSystemPath(mrc.ORMTexturePath).generic_string();
            if (!mrc.ORMTexture || mrc.ORMTexture->GetPath() != abs_orm)
                mrc.ORMTexture = AssetManager::GetTexture(abs_orm, kMeshAlbedoTextureSpec);
        }
        if (!mrc.EmissiveTexturePath.empty()) {
            std::string abs_em = Project::GetAssetFileSystemPath(mrc.EmissiveTexturePath).generic_string();
            if (!mrc.EmissiveTexture || mrc.EmissiveTexture->GetPath() != abs_em)
                mrc.EmissiveTexture = AssetManager::GetTexture(abs_em, kMeshAlbedoTextureSpec);
        }

        glm::mat4 world = scene->GetWorldTransform({ e, scene });
        Renderer3D::Submit(mrc.Mesh, mrc.AlbedoColor, mrc.AlbedoTexture,
                           mrc.ORMTexture, mrc.EmissiveTexture,
                           mrc.EmissiveFactor, world,
                           mrc.Roughness, mrc.Metallic, (int)(uint32_t)e);
    }

    static void DrawTilemapEntity(Scene* scene, entt::registry& registry, entt::entity e, TilemapComponent& tc) {
        if (tc.SpritesheetPath.empty() || tc.Tiles.empty()) return;
        std::string abs_path = Project::GetAssetFileSystemPath(tc.SpritesheetPath).generic_string();
        if (!tc.Spritesheet || tc.Spritesheet->GetPath() != abs_path)
            tc.Spritesheet = AssetManager::GetTexture(abs_path);
        if (!tc.Spritesheet) return;
        glm::mat4 world = scene->GetWorldTransform({ e, scene });
        Renderer2D::DrawTilemap(tc.Spritesheet, world,
            tc.Columns, tc.Rows, tc.TileWidth, tc.TileHeight,
            tc.SheetColumns, tc.SheetRows, tc.Tiles, (int)(uint32_t)e);
    }

    void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera, Entity selected_entity,
                                std::shared_ptr<TextureCubemap> fallback_irradiance,
                                std::shared_ptr<TextureCubemap> fallback_prefilter) {
        // Shadow pass runs first — fills the shadow map (separate FBO), then
        // restores the caller's framebuffer so the main 3D pass renders into
        // the editor viewport as usual.
        RunShadowPass(this, mRegistry, camera.GetViewMatrix(), camera.GetProjectionMatrix());

        // 3D pass — opaque meshes write depth so 2D sprites overlay correctly.
        Renderer3D::BeginScene(camera);
        GatherAndUploadLights(this, mRegistry);
        // Scene's own IBL takes priority; editor fallback fills in when the
        // scene has no environment assigned so PBR materials never go pitch-
        // black during level construction. Diffuse + specular fall back
        // independently — usually together, but the type allows for the
        // fallback HDR to fail one stage and not the other.
        auto irradiance = GetIrradianceCubemap();
        auto prefilter  = GetPrefilterCubemap();
        if (!irradiance) irradiance = std::move(fallback_irradiance);
        if (!prefilter)  prefilter  = std::move(fallback_prefilter);
        Renderer3D::SetIrradianceMap(irradiance);
        Renderer3D::SetPrefilterMap (prefilter);
        for (auto e : mRegistry.view<TransformComponent, MeshRendererComponent>()) {
            DrawMeshEntity(this, mRegistry, e, mRegistry.get<MeshRendererComponent>(e));
        }
        Renderer3D::EndScene();

        Renderer2D::BeginScene(camera);

        struct DrawCmd { float z; entt::entity e; bool tilemap; };
        std::vector<DrawCmd> draw_list;

        for (auto e : mRegistry.view<TransformComponent, SpriteRendererComponent>())
            draw_list.push_back({ mRegistry.get<TransformComponent>(e).Translation.z, e, false });
        for (auto e : mRegistry.view<TilemapComponent, TransformComponent>())
            draw_list.push_back({ mRegistry.get<TransformComponent>(e).Translation.z, e, true });

        std::sort(draw_list.begin(), draw_list.end(),
            [](const DrawCmd& a, const DrawCmd& b) { return a.z < b.z; });

        for (auto& cmd : draw_list) {
            if (cmd.tilemap) {
                DrawTilemapEntity(this, mRegistry, cmd.e, mRegistry.get<TilemapComponent>(cmd.e));
            } else {
                glm::mat4 world = GetWorldTransform({ cmd.e, this });
                DrawSprite(mRegistry, cmd.e, world, mRegistry.get<SpriteRendererComponent>(cmd.e));
            }
        }

        auto text_view = mRegistry.view<TransformComponent, TextComponent>();
        for (auto entity : text_view) {
            auto& text_comp = text_view.get<TextComponent>(entity);
            if (text_comp.FontPath.empty() || text_comp.Text.empty()) continue;
            std::string abs_path = Project::GetAssetFileSystemPath(text_comp.FontPath).generic_string();
            if (!text_comp.Font || text_comp.Font->GetPath() != abs_path)
                text_comp.Font = AssetManager::GetFont(abs_path);
            if (!text_comp.Font) continue;
            glm::mat4 world = GetWorldTransform({ entity, this })
                              * glm::scale(glm::mat4(1.0f), { text_comp.FontSize, text_comp.FontSize, 1.0f });
            Renderer2D::DrawText(text_comp.Text, text_comp.Font, world, text_comp.Color,
                                 text_comp.Kerning, text_comp.LineSpacing, (int)(uint32_t)entity);
        }

        // Particles tick in editor too so the Game Developer gets a live FX preview without entering Play.
        auto particle_view = mRegistry.view<TransformComponent, ParticleComponent>();
        for (auto entity : particle_view) {
            auto& pc = particle_view.get<ParticleComponent>(entity);
            UpdateAndDrawParticleEntity(this, mRegistry, entity, pc, ts);
        }

        auto camera_view = mRegistry.view<TransformComponent, CameraComponent>();
        for (auto entity : camera_view) {
            glm::vec3 world_pos     = glm::vec3(GetWorldTransform({ entity, this })[3]);
            glm::mat4 camera_rotation = glm::mat4(glm::mat3(glm::transpose(camera.GetViewMatrix())));
            glm::mat4 billboard       = glm::translate(glm::mat4(1.0f), world_pos)
                                      * camera_rotation
                                      * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
            Renderer2D::DrawQuad(billboard, mCameraIcon, glm::vec4(1.0f), 1.0f, (int)(uint32_t)entity);
        }

        if (selected_entity && selected_entity.HasComponent<CameraComponent>()) {
            DrawCameraFrustum(GetWorldTransform(selected_entity), selected_entity.GetComponent<CameraComponent>());
        }

        RenderPhysicsColliders();

        Renderer2D::EndScene();
    }

    // ---- Physics3D contact event plumbing ---------------------------------
    // Physics3DEventState is defined near the top of this file (DestroyEntity
    // needs it complete).

    namespace {
        // Jolt fires these from job threads; we only enqueue. Dispatch happens
        // post-Update on the main thread via Scene::DispatchPhysics3DEvents.
        class LoomContactListener3D : public JPH::ContactListener {
        public:
            explicit LoomContactListener3D(Physics3DEventState* state) : mState(state) {}

            JPH::ValidateResult OnContactValidate(const JPH::Body&, const JPH::Body&,
                                                  JPH::RVec3Arg, const JPH::CollideShapeResult&) override {
                return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
            }

            void OnContactAdded(const JPH::Body& a, const JPH::Body& b,
                                const JPH::ContactManifold&, JPH::ContactSettings&) override {
                std::lock_guard<std::mutex> lock(mState->events_mutex);
                mState->events.push_back({
                    Physics3DEventState::Kind::Begin,
                    a.GetID().GetIndexAndSequenceNumber(),
                    b.GetID().GetIndexAndSequenceNumber(),
                });
            }

            void OnContactRemoved(const JPH::SubShapeIDPair& pair) override {
                std::lock_guard<std::mutex> lock(mState->events_mutex);
                mState->events.push_back({
                    Physics3DEventState::Kind::End,
                    pair.GetBody1ID().GetIndexAndSequenceNumber(),
                    pair.GetBody2ID().GetIndexAndSequenceNumber(),
                });
            }

        private:
            Physics3DEventState* mState;
        };

        // Sensors fire OnContactAdded/Removed like normal bodies in Jolt; we
        // route them to OnSensor* if either side has a collider with IsSensor=true.
        bool IsSensorEntity(Entity e) {
            if (e.HasComponent<BoxCollider3DComponent>()     && e.GetComponent<BoxCollider3DComponent>().IsSensor)     return true;
            if (e.HasComponent<SphereCollider3DComponent>()  && e.GetComponent<SphereCollider3DComponent>().IsSensor)  return true;
            if (e.HasComponent<CapsuleCollider3DComponent>() && e.GetComponent<CapsuleCollider3DComponent>().IsSensor) return true;
            return false;
        }
    }

    // ---- Jolt helpers ------------------------------------------------------
    static JPH::Vec3 ToJolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
    static JPH::Quat EulerToJolt(const glm::vec3& e) {
        glm::quat q(e); // XYZ Euler radians -> quat
        return JPH::Quat(q.x, q.y, q.z, q.w);
    }
    static glm::vec3 FromJolt(JPH::Vec3Arg v) { return { v.GetX(), v.GetY(), v.GetZ() }; }
    static glm::vec3 JoltQuatToEuler(JPH::QuatArg q) {
        glm::quat g(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
        return glm::eulerAngles(g);
    }

    static JPH::EMotionType ToJoltMotion(Rigidbody3DComponent::BodyType t) {
        switch (t) {
            case Rigidbody3DComponent::BodyType::Static:    return JPH::EMotionType::Static;
            case Rigidbody3DComponent::BodyType::Dynamic:   return JPH::EMotionType::Dynamic;
            case Rigidbody3DComponent::BodyType::Kinematic: return JPH::EMotionType::Kinematic;
        }
        return JPH::EMotionType::Static;
    }

    void Scene::OnPhysicsStart3D() {
        if (!PhysicsEngine3D::IsInitialized()) {
            LOOM_CORE_ERROR("Scene::OnPhysicsStart3D called before PhysicsEngine3D::Init");
            return;
        }

        mPhysicsSystem3D = new JPH::PhysicsSystem();
        mPhysicsSystem3D->Init(
            /*max_bodies*/             1024,
            /*num_body_mutexes*/          0, // 0 -> Jolt picks based on thread count
            /*max_body_pairs*/         1024,
            /*max_contact_constraints*/1024,
            PhysicsEngine3D::GetBroadPhaseLayerInterface(),
            PhysicsEngine3D::GetObjectVsBroadPhaseLayerFilter(),
            PhysicsEngine3D::GetObjectLayerPairFilter()
        );
        mPhysicsSystem3D->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

        mContactListener3D = new LoomContactListener3D(mPhysics3DEvents.get());
        mPhysicsSystem3D->SetContactListener(mContactListener3D);

        JPH::BodyInterface& body_interface = mPhysicsSystem3D->GetBodyInterface();

        for (auto e : mRegistry.view<TransformComponent, Rigidbody3DComponent>()) {
            auto& transform = mRegistry.get<TransformComponent>(e);
            auto& rb        = mRegistry.get<Rigidbody3DComponent>(e);

            // Material params + offset come from the collider component.
            JPH::ShapeRefC shape;
            glm::vec3      shape_offset = { 0.0f, 0.0f, 0.0f };
            float          friction     = 0.5f;
            float          restitution  = 0.0f;
            bool           is_sensor    = false;

            // Box takes precedence if both colliders are present (an unusual setup, but well-defined).
            if (mRegistry.all_of<BoxCollider3DComponent>(e)) {
                auto& bc = mRegistry.get<BoxCollider3DComponent>(e);

                glm::vec3 scaled_extents = bc.HalfExtents * transform.Scale;
                // Jolt rejects extents below cDefaultConvexRadius (default 0.05). Clamp.
                scaled_extents = glm::max(scaled_extents, glm::vec3(0.05f));

                JPH::BoxShapeSettings box_settings(ToJolt(scaled_extents));
                box_settings.SetDensity(bc.Density);
                auto box_result = box_settings.Create();
                if (!box_result.IsValid()) {
                    LOOM_CORE_ERROR("Scene: BoxShape creation failed: {}", box_result.GetError().c_str());
                    continue;
                }
                shape        = box_result.Get();
                shape_offset = bc.Offset;
                friction     = bc.Friction;
                restitution  = bc.Restitution;
                is_sensor    = bc.IsSensor;
            } else if (mRegistry.all_of<SphereCollider3DComponent>(e)) {
                auto& sc = mRegistry.get<SphereCollider3DComponent>(e);

                // Sphere can only scale uniformly in Jolt; use the largest axis so the visual
                // collider doesn't intersect geometry the artist sees enclosed.
                float max_scale     = std::max({ transform.Scale.x, transform.Scale.y, transform.Scale.z });
                float scaled_radius = std::max(sc.Radius * max_scale, 0.05f);

                JPH::SphereShapeSettings sphere_settings(scaled_radius);
                sphere_settings.SetDensity(sc.Density);
                auto sphere_result = sphere_settings.Create();
                if (!sphere_result.IsValid()) {
                    LOOM_CORE_ERROR("Scene: SphereShape creation failed: {}", sphere_result.GetError().c_str());
                    continue;
                }
                shape        = sphere_result.Get();
                shape_offset = sc.Offset;
                friction     = sc.Friction;
                restitution  = sc.Restitution;
                is_sensor    = sc.IsSensor;
            } else if (mRegistry.all_of<CapsuleCollider3DComponent>(e)) {
                auto& cc = mRegistry.get<CapsuleCollider3DComponent>(e);

                // Capsule axis is local Y. Radius scales with the largest XZ axis
                // so a non-uniform scale doesn't invent an ellipsoid Jolt can't model.
                float xz_scale          = std::max(transform.Scale.x, transform.Scale.z);
                float scaled_radius     = std::max(cc.Radius     * xz_scale,         0.05f);
                float scaled_half_height = std::max(cc.HalfHeight * transform.Scale.y, 0.05f);

                JPH::CapsuleShapeSettings capsule_settings(scaled_half_height, scaled_radius);
                capsule_settings.SetDensity(cc.Density);
                auto capsule_result = capsule_settings.Create();
                if (!capsule_result.IsValid()) {
                    LOOM_CORE_ERROR("Scene: CapsuleShape creation failed: {}", capsule_result.GetError().c_str());
                    continue;
                }
                shape        = capsule_result.Get();
                shape_offset = cc.Offset;
                friction     = cc.Friction;
                restitution  = cc.Restitution;
                is_sensor    = cc.IsSensor;
            } else {
                LOOM_CORE_WARN("Scene: Rigidbody3D entity has no collider; body not created");
                continue;
            }

            // Wrap in RotatedTranslatedShape if the collider has an offset.
            if (shape_offset != glm::vec3(0.0f)) {
                JPH::RotatedTranslatedShapeSettings rt_settings(ToJolt(shape_offset), JPH::Quat::sIdentity(), shape);
                auto rt_result = rt_settings.Create();
                if (!rt_result.IsValid()) {
                    LOOM_CORE_ERROR("Scene: RotatedTranslatedShape creation failed: {}", rt_result.GetError().c_str());
                    continue;
                }
                shape = rt_result.Get();
            }

            JPH::ObjectLayer layer = (rb.Type == Rigidbody3DComponent::BodyType::Static)
                                     ? PhysicsLayers3D::NON_MOVING
                                     : PhysicsLayers3D::MOVING;

            JPH::BodyCreationSettings body_settings(
                shape,
                ToJolt(transform.Translation),
                EulerToJolt(transform.Rotation),
                ToJoltMotion(rb.Type),
                layer
            );
            body_settings.mFriction       = friction;
            body_settings.mRestitution    = restitution;
            body_settings.mLinearDamping  = rb.LinearDamping;
            body_settings.mAngularDamping = rb.AngularDamping;
            body_settings.mIsSensor       = is_sensor;
            body_settings.mUserData       = (uint64_t)entt::to_integral(e);
            if (rb.FixedRotation) {
                body_settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX
                                           | JPH::EAllowedDOFs::TranslationY
                                           | JPH::EAllowedDOFs::TranslationZ;
            }

            JPH::BodyID body_id = body_interface.CreateAndAddBody(
                body_settings,
                rb.Type == Rigidbody3DComponent::BodyType::Static
                    ? JPH::EActivation::DontActivate
                    : JPH::EActivation::Activate);

            rb.RuntimeBodyID = body_id.GetIndexAndSequenceNumber();
            mPhysics3DEvents->body_to_entity[rb.RuntimeBodyID] = e;
        }

        // Jolt needs this once after batch body creation for optimal broadphase queries.
        mPhysicsSystem3D->OptimizeBroadPhase();
    }

    void Scene::OnPhysicsStop3D() {
        if (!mPhysicsSystem3D) return;

        JPH::BodyInterface& body_interface = mPhysicsSystem3D->GetBodyInterface();
        for (auto e : mRegistry.view<Rigidbody3DComponent>()) {
            auto& rb = mRegistry.get<Rigidbody3DComponent>(e);
            if (rb.RuntimeBodyID == 0xffffffffu) continue;
            JPH::BodyID id(rb.RuntimeBodyID);
            body_interface.RemoveBody(id);
            body_interface.DestroyBody(id);
            rb.RuntimeBodyID = 0xffffffffu;
        }

        delete mContactListener3D;
        mContactListener3D = nullptr;

        delete mPhysicsSystem3D;
        mPhysicsSystem3D = nullptr;

        // Drop any queued contact events; the bodies they referenced are gone.
        {
            std::lock_guard<std::mutex> lock(mPhysics3DEvents->events_mutex);
            mPhysics3DEvents->events.clear();
        }
        mPhysics3DEvents->body_to_entity.clear();
    }

    // Aggregates a tilemap's solid cells into merged collision rectangles.
    // Shared by the runtime fixture generator and the debug overlay so the
    // editor previews the exact shapes Box2D will collide against.
    //
    // Two-pass: (1) per-row greedy horizontal runs, (2) extend a run from
    // the previous row down by one if it has the same ColStart + Count.
    // Catches rectangular regions (the common solid-floor / wall-block
    // case) without paying for true max-rectangle decomposition.
    //
    // Staggered regions (e.g. an L-shape) still emit one rect per row of
    // the staggered part — same physical behavior as the row-only version,
    // just no improvement there. Real games rarely have such shapes.
    struct TilemapColliderRect {
        int RowStart;  // inclusive
        int RowEnd;    // inclusive (== RowStart for a single-row rect)
        int ColStart;  // inclusive
        int Count;     // columns; rect spans [ColStart, ColStart + Count)
    };

    template <typename SolidPredicate>
    static std::vector<TilemapColliderRect> ComputeTilemapColliderRects(
        int rows, int cols, SolidPredicate&& is_solid) {

        struct OpenRect { int RowStart; int ColStart; int Count; };
        std::vector<TilemapColliderRect> out;
        std::vector<OpenRect>            prev_open;  // runs from row r-1 still eligible to grow
        std::vector<OpenRect>            cur_open;   // runs from row r

        for (int r = 0; r < rows; ++r) {
            cur_open.clear();

            int c = 0;
            while (c < cols) {
                if (!is_solid(r, c)) { ++c; continue; }
                int start = c;
                while (c < cols && is_solid(r, c)) ++c;
                int count = c - start;

                // Try to fuse with a matching open run from the previous row.
                // Linear scan is fine — runs per row are typically a handful.
                auto it = std::find_if(prev_open.begin(), prev_open.end(),
                    [&](const OpenRect& o) { return o.ColStart == start && o.Count == count; });
                if (it != prev_open.end()) {
                    cur_open.push_back({ it->RowStart, it->ColStart, it->Count });
                    prev_open.erase(it);
                } else {
                    cur_open.push_back({ r, start, count });
                }
            }

            // Anything left in prev_open didn't extend into this row — emit.
            for (const auto& o : prev_open) {
                out.push_back({ o.RowStart, r - 1, o.ColStart, o.Count });
            }
            prev_open.swap(cur_open);
        }

        // Flush remaining open rects at end of grid.
        int last_row = rows - 1;
        for (const auto& o : prev_open) {
            out.push_back({ o.RowStart, last_row, o.ColStart, o.Count });
        }

        return out;
    }

    void Scene::OnRuntimeStart() {
        mRegistry.view<AnimationComponent>().each([](AnimationComponent& anim) {
            anim.CurrentFrame   = 0;
            anim.ElapsedTime    = 0.0f;
            anim.LastEventFrame = -1;
        });

        mRegistry.view<ParticleComponent>().each([](ParticleComponent& pc) {
            pc.Live.clear();
            pc.SpawnAccumulator = 0.0f;
        });

        ScriptingEngine::OnRuntimeStart(this);

        // Autoplay audio sources
        auto audio_view = mRegistry.view<AudioSourceComponent>();
        for (auto e : audio_view) {
            Entity entity = { e, this };
            auto& asc = entity.GetComponent<AudioSourceComponent>();
            if (asc.AutoPlay && !asc.AssetPath.empty()) {
                std::string full = Project::GetAssetFileSystemPath(asc.AssetPath).generic_string();
                AudioEngine::PlaySource(asc, full);
            }
        }

        b2WorldDef world_def = b2DefaultWorldDef();
        world_def.gravity = (b2Vec2){ 0.0f, -9.8f };
        world_def.contactHertz = 120.0f;
        world_def.contactDampingRatio = 10.0f;
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
            body_def.userData = reinterpret_cast<void*>(static_cast<uintptr_t>(entt::to_integral(e)));

            rb2d.RuntimeBody = b2CreateBody(mPhysicsWorld, &body_def);

            if (entity.HasComponent<BoxCollider2DComponent>()) {
                auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();

                b2ShapeDef shape_def = b2DefaultShapeDef();
                shape_def.density = bc2d.Density;
                shape_def.material.friction = bc2d.Friction;
                shape_def.material.restitution = bc2d.Restitution;
                // Box2D 3.1: a sensor only detects a visitor shape if that
                // visitor also opts in to sensor events — so enable it on
                // every shape, sensor or not.
                shape_def.enableSensorEvents = true;
                if (bc2d.IsSensor) {
                    shape_def.isSensor = true;
                } else {
                    shape_def.enableContactEvents = true;
                }

                b2Polygon box = b2MakeOffsetBox(
                    bc2d.Size.x * transform.Scale.x,
                    bc2d.Size.y * transform.Scale.y,
                    { bc2d.Offset.x, bc2d.Offset.y },
                    b2MakeRot(0.0f)
                );

                bc2d.RuntimeFixture = b2CreatePolygonShape(rb2d.RuntimeBody, &shape_def, &box);
            }

            if (entity.HasComponent<CircleCollider2DComponent>()) {
                auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();

                b2ShapeDef shape_def = b2DefaultShapeDef();
                shape_def.density = cc2d.Density;
                shape_def.material.friction = cc2d.Friction;
                shape_def.material.restitution = cc2d.Restitution;
                // Box2D 3.1: a sensor only detects a visitor shape if that
                // visitor also opts in to sensor events — so enable it on
                // every shape, sensor or not.
                shape_def.enableSensorEvents = true;
                if (cc2d.IsSensor) {
                    shape_def.isSensor = true;
                } else {
                    shape_def.enableContactEvents = true;
                }

                b2Circle circle;
                circle.center = { cc2d.Offset.x, cc2d.Offset.y };
                circle.radius = cc2d.Radius * transform.Scale.x;

                cc2d.RuntimeFixture = b2CreateCircleShape(rb2d.RuntimeBody, &shape_def, &circle);
            }
        }

        // Tilemap colliders — one static body per tilemap entity, with greedy
        // horizontal-run merged box fixtures for every cell whose sheet tile is
        // marked Solid. Greedy-merging keeps fixture counts low for typical
        // platformer floors/walls.
        auto tilemap_view = mRegistry.view<TilemapComponent, TransformComponent>();
        for (auto e : tilemap_view) {
            auto& tc = mRegistry.get<TilemapComponent>(e);
            auto& tr = mRegistry.get<TransformComponent>(e);

            if (tc.Tiles.empty() || tc.Solid.empty()) continue;
            int sheet_total = tc.SheetColumns * tc.SheetRows;
            // Defense against drifted Solid table (e.g. SheetCols/Rows changed after authoring).
            if ((int)tc.Solid.size() != sheet_total) continue;

            auto is_solid = [&](int r, int c) -> bool {
                int idx = tc.Tiles[r * tc.Columns + c];
                return idx >= 0 && idx < sheet_total && tc.Solid[idx];
            };

            bool has_any_solid = false;
            for (int r = 0; r < tc.Rows && !has_any_solid; ++r)
                for (int c = 0; c < tc.Columns && !has_any_solid; ++c)
                    if (is_solid(r, c)) has_any_solid = true;
            if (!has_any_solid) continue;

            b2BodyDef body_def = b2DefaultBodyDef();
            body_def.type     = b2_staticBody;
            body_def.position = { tr.Translation.x, tr.Translation.y };
            body_def.rotation = b2MakeRot(tr.Rotation.z);
            body_def.userData = reinterpret_cast<void*>(static_cast<uintptr_t>(entt::to_integral(e)));
            tc.RuntimeBody    = b2CreateBody(mPhysicsWorld, &body_def);

            const float hw = tc.Columns * tc.TileWidth  * 0.5f;
            const float hh = tc.Rows    * tc.TileHeight * 0.5f;

            auto rects = ComputeTilemapColliderRects(tc.Rows, tc.Columns, is_solid);
            for (const auto& rect : rects) {
                int   span_rows = rect.RowEnd - rect.RowStart + 1;
                float box_hw    = rect.Count * tc.TileWidth  * 0.5f;
                float box_hh    = span_rows  * tc.TileHeight * 0.5f;
                float center_x  = -hw + (rect.ColStart + rect.Count   * 0.5f) * tc.TileWidth;
                float center_y  =  hh - (rect.RowStart + span_rows    * 0.5f) * tc.TileHeight;

                b2ShapeDef shape_def = b2DefaultShapeDef();
                shape_def.enableContactEvents = true;
                b2Polygon box = b2MakeOffsetBox(box_hw, box_hh,
                                                { center_x, center_y }, b2MakeRot(0.0f));
                b2CreatePolygonShape(tc.RuntimeBody, &shape_def, &box);
            }
        }

        OnPhysicsStart3D();
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
            int32_t sub_step_count = 8;
            b2World_Step(mPhysicsWorld, ts, sub_step_count);

            // Resolve a Box2D shape back to an Entity, validating every hop:
            // a shape/body destroyed mid-dispatch (e.g. a script calling
            // entity:Destroy()) must not resurrect a stale entity handle.
            auto resolve_shape = [this](b2ShapeId shape_id) -> Entity {
                if (!b2Shape_IsValid(shape_id)) return {};
                b2BodyId body_id = b2Shape_GetBody(shape_id);
                if (!b2Body_IsValid(body_id)) return {};
                void* userdata = b2Body_GetUserData(body_id);
                auto  raw      = static_cast<entt::id_type>(reinterpret_cast<uintptr_t>(userdata));
                entt::entity e = static_cast<entt::entity>(raw);
                if (!mRegistry.valid(e)) return {};
                return { e, this };
            };

            b2ContactEvents contact_events = b2World_GetContactEvents(mPhysicsWorld);
            for (int i = 0; i < contact_events.beginCount; ++i) {
                Entity a = resolve_shape(contact_events.beginEvents[i].shapeIdA);
                Entity b = resolve_shape(contact_events.beginEvents[i].shapeIdB);
                if (a && b) ScriptingEngine::OnCollisionBegin(a, b);
            }
            for (int i = 0; i < contact_events.endCount; ++i) {
                Entity a = resolve_shape(contact_events.endEvents[i].shapeIdA);
                Entity b = resolve_shape(contact_events.endEvents[i].shapeIdB);
                if (a && b) ScriptingEngine::OnCollisionEnd(a, b);
            }

            b2SensorEvents sensor_events = b2World_GetSensorEvents(mPhysicsWorld);
            for (int i = 0; i < sensor_events.beginCount; ++i) {
                Entity a = resolve_shape(sensor_events.beginEvents[i].sensorShapeId);
                Entity b = resolve_shape(sensor_events.beginEvents[i].visitorShapeId);
                if (a && b) ScriptingEngine::OnSensorBegin(a, b);
            }
            for (int i = 0; i < sensor_events.endCount; ++i) {
                Entity a = resolve_shape(sensor_events.endEvents[i].sensorShapeId);
                Entity b = resolve_shape(sensor_events.endEvents[i].visitorShapeId);
                if (a && b) ScriptingEngine::OnSensorEnd(a, b);
            }

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

        // 2b. Update 3D physics
        if (mPhysicsSystem3D) {
            const int collision_steps = 1;
            mPhysicsSystem3D->Update(ts, collision_steps,
                                     &PhysicsEngine3D::GetTempAllocator(),
                                     &PhysicsEngine3D::GetJobSystem());

            DispatchPhysics3DEvents();

            JPH::BodyInterface& body_interface = mPhysicsSystem3D->GetBodyInterface();
            for (auto e : mRegistry.view<TransformComponent, Rigidbody3DComponent>()) {
                auto& rb = mRegistry.get<Rigidbody3DComponent>(e);
                if (rb.RuntimeBodyID == 0xffffffffu) continue;
                if (rb.Type == Rigidbody3DComponent::BodyType::Static) continue;

                JPH::BodyID id(rb.RuntimeBodyID);
                JPH::Vec3   pos = body_interface.GetPosition(id);
                JPH::Quat   rot = body_interface.GetRotation(id);

                auto& transform     = mRegistry.get<TransformComponent>(e);
                transform.Translation = FromJolt(pos);
                transform.Rotation    = JoltQuatToEuler(rot);
            }
        }

        // 3. Advance sprite animations + fire per-frame events
        mRegistry.view<AnimationComponent>().each([&](auto entt_id, AnimationComponent& anim) {
            const AnimationClip* clip = anim.GetCurrentClip();
            if (!clip || clip->Frames.empty()) return;

            if (anim.IsPlaying) {
                anim.ElapsedTime += ts;
                while (anim.ElapsedTime >= clip->FrameDuration) {
                    anim.ElapsedTime -= clip->FrameDuration;
                    anim.CurrentFrame++;
                    if (anim.CurrentFrame >= (int)clip->Frames.size()) {
                        if (clip->Loop) anim.CurrentFrame = 0;
                        else { anim.CurrentFrame = (int)clip->Frames.size() - 1; anim.IsPlaying = false; }
                    }
                }
            }

            // Fire events whenever the visible frame changes (or on initial Play, when
            // LastEventFrame == -1). Skips when nothing changed.
            if (anim.LastEventFrame != anim.CurrentFrame) {
                anim.LastEventFrame = anim.CurrentFrame;
                if (mRegistry.all_of<LuaScriptComponent>(entt_id)) {
                    Entity entity{ entt_id, this };
                    for (const auto& ev : clip->Events) {
                        if (ev.Frame == anim.CurrentFrame)
                            ScriptingEngine::OnAnimationEvent(entity, ev.Name);
                    }
                }
            }
        });

        // 4. Find the primary camera
        Camera*   main_camera = nullptr;
        glm::mat4 camera_transform;

        auto view = mRegistry.view<TransformComponent, CameraComponent>();
        for (auto entity : view) {
            auto& camera = view.get<CameraComponent>(entity);
            if (camera.Primary) {
                main_camera      = &camera.Camera;
                camera_transform = GetWorldTransform({ entity, this });
                break;
            }
        }

        if (main_camera) {
            // Skybox — play mode uses ONLY the scene's own environment. No
            // editor fallback: if the level was authored with no skybox, the
            // shipped game shows the framebuffer clear color (and meshes get
            // zero IBL ambient).
            if (auto scene_skybox = GetSkyboxCubemap()) {
                Renderer3D::DrawSkybox(glm::inverse(camera_transform),
                                       main_camera->GetProjectionMatrix(),
                                       scene_skybox);
            }

            // Shadow pass first — runs even in Play mode so cast shadows are part
            // of the shipped experience, not just an editor preview.
            RunShadowPass(this, mRegistry,
                          glm::inverse(camera_transform),
                          main_camera->GetProjectionMatrix());

            // 3D pass — opaque meshes write depth so 2D sprites overlay correctly.
            Renderer3D::BeginScene(*main_camera, camera_transform);
            GatherAndUploadLights(this, mRegistry);
            Renderer3D::SetIrradianceMap(GetIrradianceCubemap());
            Renderer3D::SetPrefilterMap (GetPrefilterCubemap());
            for (auto e : mRegistry.view<TransformComponent, MeshRendererComponent>()) {
                DrawMeshEntity(this, mRegistry, e, mRegistry.get<MeshRendererComponent>(e));
            }
            Renderer3D::EndScene();

            Renderer2D::BeginScene(*main_camera, camera_transform);

            struct DrawCmd { float z; entt::entity e; bool tilemap; };
            std::vector<DrawCmd> draw_list;

            for (auto e : mRegistry.view<TransformComponent, SpriteRendererComponent>())
                draw_list.push_back({ mRegistry.get<TransformComponent>(e).Translation.z, e, false });
            for (auto e : mRegistry.view<TilemapComponent, TransformComponent>())
                draw_list.push_back({ mRegistry.get<TransformComponent>(e).Translation.z, e, true });

            std::sort(draw_list.begin(), draw_list.end(),
                [](const DrawCmd& a, const DrawCmd& b) { return a.z < b.z; });

            for (auto& cmd : draw_list) {
                if (cmd.tilemap) {
                    DrawTilemapEntity(this, mRegistry, cmd.e, mRegistry.get<TilemapComponent>(cmd.e));
                } else {
                    glm::mat4 world = GetWorldTransform({ cmd.e, this });
                    DrawSprite(mRegistry, cmd.e, world, mRegistry.get<SpriteRendererComponent>(cmd.e));
                }
            }

            auto text_view = mRegistry.view<TransformComponent, TextComponent>();
            for (auto entity : text_view) {
                auto& text_comp = text_view.get<TextComponent>(entity);
                if (text_comp.FontPath.empty() || text_comp.Text.empty()) continue;
                std::string abs_path = Project::GetAssetFileSystemPath(text_comp.FontPath).generic_string();
                if (!text_comp.Font || text_comp.Font->GetPath() != abs_path)
                    text_comp.Font = AssetManager::GetFont(abs_path);
                if (!text_comp.Font) continue;
                glm::mat4 world = GetWorldTransform({ entity, this })
                                  * glm::scale(glm::mat4(1.0f), { text_comp.FontSize, text_comp.FontSize, 1.0f });
                Renderer2D::DrawText(text_comp.Text, text_comp.Font, world, text_comp.Color,
                                     text_comp.Kerning, text_comp.LineSpacing, (int)(uint32_t)entity);
            }

            auto particle_view = mRegistry.view<TransformComponent, ParticleComponent>();
            for (auto entity : particle_view) {
                auto& pc = particle_view.get<ParticleComponent>(entity);
                UpdateAndDrawParticleEntity(this, mRegistry, entity, pc, ts);
            }

            RenderPhysicsColliders();

            Renderer2D::EndScene();
        }
    }

    void Scene::OnRuntimeStop() {
        mRegistry.view<AnimationComponent>().each([](AnimationComponent& anim) {
            anim.CurrentFrame   = 0;
            anim.ElapsedTime    = 0.0f;
            anim.IsPlaying      = true;
            anim.LastEventFrame = -1;
        });

        mRegistry.view<ParticleComponent>().each([](ParticleComponent& pc) {
            pc.Live.clear();
            pc.SpawnAccumulator = 0.0f;
        });

        ScriptingEngine::OnRuntimeStop();

        // Stop all audio sources
        mRegistry.view<AudioSourceComponent>().each([](AudioSourceComponent& asc) {
            AudioEngine::StopSource(asc);
        });

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

        // Reset tilemap body handles — fixtures were owned by the destroyed world.
        mRegistry.view<TilemapComponent>().each([](TilemapComponent& tc) {
            tc.RuntimeBody = b2_nullBodyId;
        });

        OnPhysicsStop3D();
    }

    void Scene::DrawCameraFrustum(const glm::mat4& world, const CameraComponent& camera_component) {
        const SceneCamera& camera    = camera_component.Camera;
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

    static void DrawWireCircle3D(const glm::vec3& center, const glm::vec3& basis_a, const glm::vec3& basis_b,
                                 float radius, int segments, const glm::vec4& color, int entity_id) {
        glm::vec3 prev = center + basis_a * radius;
        for (int i = 1; i <= segments; ++i) {
            float t = (float)i / (float)segments * 6.2831853f;
            glm::vec3 next = center + (basis_a * std::cos(t) + basis_b * std::sin(t)) * radius;
            Renderer2D::DrawLine(prev, next, color, entity_id);
            prev = next;
        }
    }

    static void DrawWireBox3D(const glm::vec3& center, const glm::vec3& axis_x, const glm::vec3& axis_y,
                              const glm::vec3& axis_z, const glm::vec3& half_extents,
                              const glm::vec4& color, int entity_id) {
        glm::vec3 corners[8];
        for (int i = 0; i < 8; ++i) {
            glm::vec3 s{ (i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f };
            corners[i] = center
                       + axis_x * (s.x * half_extents.x)
                       + axis_y * (s.y * half_extents.y)
                       + axis_z * (s.z * half_extents.z);
        }
        // 4 edges along X (corners differ only in bit 0), 4 along Y (bit 1), 4 along Z (bit 2)
        const int edges[12][2] = {
            {0,1},{2,3},{4,5},{6,7},   // X
            {0,2},{1,3},{4,6},{5,7},   // Y
            {0,4},{1,5},{2,6},{3,7},   // Z
        };
        for (auto& e : edges)
            Renderer2D::DrawLine(corners[e[0]], corners[e[1]], color, entity_id);
    }

    void Scene::RenderPhysicsColliders() {
        if (!mShowPhysicsColliders) return;

        glm::vec4 collider_color = { 0.1f, 0.9f, 0.1f, 1.0f };

        // ---- 3D colliders (wireframes via Renderer2D::DrawLine) ----
        for (auto e : mRegistry.view<TransformComponent, BoxCollider3DComponent>()) {
            auto& transform = mRegistry.get<TransformComponent>(e);
            auto& bc        = mRegistry.get<BoxCollider3DComponent>(e);
            glm::quat  q    = glm::quat(transform.Rotation);
            glm::vec3  ax   = q * glm::vec3(1, 0, 0);
            glm::vec3  ay   = q * glm::vec3(0, 1, 0);
            glm::vec3  az   = q * glm::vec3(0, 0, 1);
            glm::vec3  center = transform.Translation + q * bc.Offset;
            glm::vec3  half_extents = bc.HalfExtents * transform.Scale;
            DrawWireBox3D(center, ax, ay, az, half_extents, collider_color, (int)(uint32_t)e);
        }

        for (auto e : mRegistry.view<TransformComponent, SphereCollider3DComponent>()) {
            auto& transform = mRegistry.get<TransformComponent>(e);
            auto& sc        = mRegistry.get<SphereCollider3DComponent>(e);
            glm::quat q     = glm::quat(transform.Rotation);
            glm::vec3 center = transform.Translation + q * sc.Offset;
            float     radius = sc.Radius * std::max({ transform.Scale.x, transform.Scale.y, transform.Scale.z });
            // Three world-axis great circles — sphere itself is rotation-invariant.
            int eid = (int)(uint32_t)e;
            DrawWireCircle3D(center, glm::vec3(1,0,0), glm::vec3(0,1,0), radius, 24, collider_color, eid);
            DrawWireCircle3D(center, glm::vec3(0,1,0), glm::vec3(0,0,1), radius, 24, collider_color, eid);
            DrawWireCircle3D(center, glm::vec3(1,0,0), glm::vec3(0,0,1), radius, 24, collider_color, eid);
        }

        for (auto e : mRegistry.view<TransformComponent, CapsuleCollider3DComponent>()) {
            auto& transform = mRegistry.get<TransformComponent>(e);
            auto& cc        = mRegistry.get<CapsuleCollider3DComponent>(e);
            glm::quat q     = glm::quat(transform.Rotation);
            glm::vec3 ay    = q * glm::vec3(0, 1, 0); // capsule's local Y axis
            glm::vec3 ax    = q * glm::vec3(1, 0, 0);
            glm::vec3 az    = q * glm::vec3(0, 0, 1);
            glm::vec3 center = transform.Translation + q * cc.Offset;
            float xz_scale = std::max(transform.Scale.x, transform.Scale.z);
            float radius   = cc.Radius     * xz_scale;
            float half_h   = cc.HalfHeight * transform.Scale.y;
            glm::vec3 top    = center + ay * half_h;
            glm::vec3 bottom = center - ay * half_h;
            int eid = (int)(uint32_t)e;
            // Two end caps + middle ring (around capsule's Y axis)
            DrawWireCircle3D(top,    ax, az, radius, 24, collider_color, eid);
            DrawWireCircle3D(bottom, ax, az, radius, 24, collider_color, eid);
            DrawWireCircle3D(center, ax, az, radius, 24, collider_color, eid);
            // Two side outlines (cylinder body), one per perpendicular axis
            Renderer2D::DrawLine(top + ax * radius, bottom + ax * radius, collider_color, eid);
            Renderer2D::DrawLine(top - ax * radius, bottom - ax * radius, collider_color, eid);
            Renderer2D::DrawLine(top + az * radius, bottom + az * radius, collider_color, eid);
            Renderer2D::DrawLine(top - az * radius, bottom - az * radius, collider_color, eid);
            // Hemispherical end-cap outlines (semicircles in the XY and YZ planes)
            DrawWireCircle3D(top,    ax, ay, radius, 24, collider_color, eid);
            DrawWireCircle3D(top,    az, ay, radius, 24, collider_color, eid);
            DrawWireCircle3D(bottom, ax, ay, radius, 24, collider_color, eid);
            DrawWireCircle3D(bottom, az, ay, radius, 24, collider_color, eid);
        }

        // ---- 2D colliders (existing) ----
        auto box_view = mRegistry.view<TransformComponent, BoxCollider2DComponent>();
        for (auto entity : box_view) {
            auto [transform, bc2d] = box_view.get<TransformComponent, BoxCollider2DComponent>(entity);

            glm::vec3 translation = transform.Translation + glm::vec3(bc2d.Offset, 0.001f);
            glm::vec3 scale = transform.Scale * glm::vec3(bc2d.Size * 2.0f, 1.0f);

            glm::mat4 transform_mat = glm::translate(glm::mat4(1.0f), translation)
                                    * glm::rotate(glm::mat4(1.0f), transform.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
                                    * glm::scale(glm::mat4(1.0f), scale);

            glm::vec3 p0 = transform_mat * glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f);
            glm::vec3 p1 = transform_mat * glm::vec4( 0.5f, -0.5f, 0.0f, 1.0f);
            glm::vec3 p2 = transform_mat * glm::vec4( 0.5f,  0.5f, 0.0f, 1.0f);
            glm::vec3 p3 = transform_mat * glm::vec4(-0.5f,  0.5f, 0.0f, 1.0f);

            int entity_id = (int)(uint32_t)entity;
            Renderer2D::DrawLine(p0, p1, collider_color, entity_id);
            Renderer2D::DrawLine(p1, p2, collider_color, entity_id);
            Renderer2D::DrawLine(p2, p3, collider_color, entity_id);
            Renderer2D::DrawLine(p3, p0, collider_color, entity_id);
        }

        auto circle_view = mRegistry.view<TransformComponent, CircleCollider2DComponent>();
        for (auto entity : circle_view) {
            auto [transform, cc2d] = circle_view.get<TransformComponent, CircleCollider2DComponent>(entity);

            glm::vec3 translation = transform.Translation + glm::vec3(cc2d.Offset, 0.001f);
            float diameter = cc2d.Radius * transform.Scale.x * 2.0f;

            glm::mat4 transform_mat = glm::translate(glm::mat4(1.0f), translation)
                                    * glm::scale(glm::mat4(1.0f), glm::vec3(diameter));

            Renderer2D::DrawCircle(transform_mat, collider_color, 0.05f, 0.005f, (int)(uint32_t)entity);
        }

        // ---- Tilemap collider rectangles — re-runs the greedy walk used at physics start
        // so the editor sees the exact same merged boxes Box2D will collide against.
        // Visible in Edit mode too, which is the whole point: artists can tweak the Solid
        // flags and see the resulting rectangles without entering Play.
        auto tilemap_view2 = mRegistry.view<TransformComponent, TilemapComponent>();
        for (auto e : tilemap_view2) {
            auto& transform = mRegistry.get<TransformComponent>(e);
            auto& tc        = mRegistry.get<TilemapComponent>(e);
            if (tc.Tiles.empty() || tc.Solid.empty()) continue;
            int sheet_total = tc.SheetColumns * tc.SheetRows;
            if ((int)tc.Solid.size() != sheet_total) continue;

            auto is_solid = [&](int r, int c) -> bool {
                int idx = tc.Tiles[r * tc.Columns + c];
                return idx >= 0 && idx < sheet_total && tc.Solid[idx];
            };

            const float hw = tc.Columns * tc.TileWidth  * 0.5f;
            const float hh = tc.Rows    * tc.TileHeight * 0.5f;
            glm::mat4 base = glm::translate(glm::mat4(1.0f), transform.Translation)
                           * glm::rotate(glm::mat4(1.0f), transform.Rotation.z, { 0, 0, 1 });
            int eid = (int)(uint32_t)e;

            auto rects = ComputeTilemapColliderRects(tc.Rows, tc.Columns, is_solid);
            for (const auto& rect : rects) {
                int   span_rows = rect.RowEnd - rect.RowStart + 1;
                float box_hw    = rect.Count * tc.TileWidth  * 0.5f;
                float box_hh    = span_rows  * tc.TileHeight * 0.5f;
                float cx        = -hw + (rect.ColStart + rect.Count   * 0.5f) * tc.TileWidth;
                float cy        =  hh - (rect.RowStart + span_rows    * 0.5f) * tc.TileHeight;

                glm::vec3 p0 = base * glm::vec4(cx - box_hw, cy - box_hh, 0.001f, 1.0f);
                glm::vec3 p1 = base * glm::vec4(cx + box_hw, cy - box_hh, 0.001f, 1.0f);
                glm::vec3 p2 = base * glm::vec4(cx + box_hw, cy + box_hh, 0.001f, 1.0f);
                glm::vec3 p3 = base * glm::vec4(cx - box_hw, cy + box_hh, 0.001f, 1.0f);

                Renderer2D::DrawLine(p0, p1, collider_color, eid);
                Renderer2D::DrawLine(p1, p2, collider_color, eid);
                Renderer2D::DrawLine(p2, p3, collider_color, eid);
                Renderer2D::DrawLine(p3, p0, collider_color, eid);
            }
        }
    }

    Scene::RaycastHit2D Scene::Raycast2D(glm::vec2 origin, glm::vec2 direction, float distance) {
        RaycastHit2D result;
        if (!b2World_IsValid(mPhysicsWorld))
            return result;

        glm::vec2 translation = glm::normalize(direction) * distance;
        b2Vec2 b2_origin      = { origin.x, origin.y };
        b2Vec2 b2_translation = { translation.x, translation.y };

        b2QueryFilter filter  = b2DefaultQueryFilter();
        b2RayResult   ray     = b2World_CastRayClosest(mPhysicsWorld, b2_origin, b2_translation, filter);
        if (!ray.hit)
            return result;

        result.hit    = true;
        result.point  = { ray.point.x,  ray.point.y  };
        result.normal = { ray.normal.x, ray.normal.y };

        b2BodyId body_id      = b2Shape_GetBody(ray.shapeId);
        void*    userdata     = b2Body_GetUserData(body_id);
        auto     raw          = static_cast<entt::id_type>(reinterpret_cast<uintptr_t>(userdata));
        result.entityHandle   = static_cast<entt::entity>(raw);
        return result;
    }

    // Callback fires once per overlapping shape; we deduplicate by body to avoid
    // returning the same entity twice when it has multiple collider components.
    struct OverlapContext {
        std::vector<entt::entity>         results;
        std::unordered_set<entt::entity>  seen;
    };

    static bool OverlapCallback(b2ShapeId shape_id, void* context) {
        auto& ctx    = *static_cast<OverlapContext*>(context);
        b2BodyId bid = b2Shape_GetBody(shape_id);
        void*    ud  = b2Body_GetUserData(bid);
        auto     raw = static_cast<entt::id_type>(reinterpret_cast<uintptr_t>(ud));
        auto     e   = static_cast<entt::entity>(raw);
        if (ctx.seen.insert(e).second)
            ctx.results.push_back(e);
        return true; // continue query
    }

    std::vector<entt::entity> Scene::OverlapCircle2D(glm::vec2 center, float radius) {
        if (!b2World_IsValid(mPhysicsWorld)) return {};

        OverlapContext context;

        b2Vec2       point{ center.x, center.y };
        b2ShapeProxy proxy = b2MakeProxy(&point, 1, radius);
        b2World_OverlapShape(mPhysicsWorld, &proxy, b2DefaultQueryFilter(), OverlapCallback, &context);
        return context.results;
    }

    void Scene::DispatchPhysics3DEvents() {
        // Swap out the queue under the lock so dispatch (which can call into Lua,
        // which can spawn entities, etc.) doesn't hold the mutex.
        std::vector<Physics3DEventState::Event> drained;
        {
            std::lock_guard<std::mutex> lock(mPhysics3DEvents->events_mutex);
            drained.swap(mPhysics3DEvents->events);
        }

        auto resolve = [this](uint32_t body_id) -> Entity {
            auto it = mPhysics3DEvents->body_to_entity.find(body_id);
            if (it == mPhysics3DEvents->body_to_entity.end()) return {};
            if (!mRegistry.valid(it->second)) return {}; // entity destroyed mid-dispatch
            return Entity{ it->second, this };
        };

        for (const auto& ev : drained) {
            Entity a = resolve(ev.body_a);
            Entity b = resolve(ev.body_b);
            if (!a || !b) continue;

            bool sensor = IsSensorEntity(a) || IsSensorEntity(b);
            if (ev.kind == Physics3DEventState::Kind::Begin) {
                if (sensor) ScriptingEngine::OnSensorBegin(a, b);
                else        ScriptingEngine::OnCollisionBegin(a, b);
            } else {
                if (sensor) ScriptingEngine::OnSensorEnd(a, b);
                else        ScriptingEngine::OnCollisionEnd(a, b);
            }
        }
    }

    // ---- 3D physics runtime helpers ---------------------------------------

    static JPH::BodyID ResolveBody3D(Scene* scene, JPH::PhysicsSystem* system, Entity entity) {
        if (!system || !entity || !entity.HasComponent<Rigidbody3DComponent>())
            return {};
        auto& rb = entity.GetComponent<Rigidbody3DComponent>();
        if (rb.RuntimeBodyID == 0xffffffffu) return {};
        return JPH::BodyID(rb.RuntimeBodyID);
    }

    void Scene::SetLinearVelocity3D(Entity entity, const glm::vec3& v) {
        JPH::BodyID id = ResolveBody3D(this, mPhysicsSystem3D, entity);
        if (id.IsInvalid()) return;
        // Wakes the body so the change takes effect immediately.
        mPhysicsSystem3D->GetBodyInterface().SetLinearVelocity(id, ToJolt(v));
    }

    glm::vec3 Scene::GetLinearVelocity3D(Entity entity) {
        JPH::BodyID id = ResolveBody3D(this, mPhysicsSystem3D, entity);
        if (id.IsInvalid()) return {};
        return FromJolt(mPhysicsSystem3D->GetBodyInterface().GetLinearVelocity(id));
    }

    void Scene::ApplyForce3D(Entity entity, const glm::vec3& f) {
        JPH::BodyID id = ResolveBody3D(this, mPhysicsSystem3D, entity);
        if (id.IsInvalid()) return;
        mPhysicsSystem3D->GetBodyInterface().AddForce(id, ToJolt(f));
    }

    void Scene::ApplyImpulse3D(Entity entity, const glm::vec3& j) {
        JPH::BodyID id = ResolveBody3D(this, mPhysicsSystem3D, entity);
        if (id.IsInvalid()) return;
        mPhysicsSystem3D->GetBodyInterface().AddImpulse(id, ToJolt(j));
    }

    std::vector<entt::entity> Scene::OverlapBox2D(glm::vec2 center, glm::vec2 half_extents) {
        if (!b2World_IsValid(mPhysicsWorld)) return {};

        OverlapContext context;

        b2Vec2 points[4];
        points[0] = { center.x - half_extents.x, center.y - half_extents.y };
        points[1] = { center.x + half_extents.x, center.y - half_extents.y };
        points[2] = { center.x + half_extents.x, center.y + half_extents.y };
        points[3] = { center.x - half_extents.x, center.y + half_extents.y };
        b2ShapeProxy proxy = b2MakeProxy(points, 4, 0.0f);
        b2World_OverlapShape(mPhysicsWorld, &proxy, b2DefaultQueryFilter(), OverlapCallback, &context);
        return context.results;
    }

} // namespace Loom
