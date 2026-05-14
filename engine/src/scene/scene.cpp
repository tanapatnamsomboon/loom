#include "loom/scene/scene.h"
#include "loom/asset/asset_manager.h"
#include "loom/audio/audio_engine.h"
#include "loom/core/uuid.h"
#include "loom/project/project.h"
#include "loom/renderer/renderer_2d.h"
#include "loom/renderer/renderer_3d.h"
#include "loom/scene/components.h"
#include "loom/scene/entity.h"
#include "loom/scripting/scripting_engine.h"
#include <algorithm>
#include <random>
#include <unordered_set>
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

        mEntityMap.erase(entity.GetComponent<IDComponent>().ID);
        mRegistry.destroy(entity);
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
            if (!anim.Frames.empty() && sprite.Texture) {
                int frame_idx = std::clamp(anim.CurrentFrame, 0, (int)anim.Frames.size() - 1);
                const auto& uv = anim.Frames[frame_idx];
                const glm::vec2 tex_coords[4] = {
                    { uv.x, uv.y }, { uv.z, uv.y }, { uv.z, uv.w }, { uv.x, uv.w }
                };
                Renderer2D::DrawQuad(world, sprite.Texture, tex_coords, sprite.Color, (int)entt::to_entity(e));
                return;
            }
        }
        if (sprite.Texture)
            Renderer2D::DrawQuad(world, sprite.Texture, sprite.Color, sprite.TilingFactor, (int)entt::to_entity(e));
        else
            Renderer2D::DrawQuad(world, sprite.Color, (int)entt::to_entity(e));
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
        DrawParticles(pc, world, (int)entt::to_entity(e));
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
                mrc.AlbedoTexture = AssetManager::GetTexture(abs_tex);
        }

        glm::mat4 world = scene->GetWorldTransform({ e, scene });
        Renderer3D::Submit(mrc.Mesh, mrc.AlbedoColor, mrc.AlbedoTexture, world,
                           mrc.Roughness, mrc.Metallic, (int)entt::to_entity(e));
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
            tc.SheetColumns, tc.SheetRows, tc.Tiles, (int)entt::to_entity(e));
    }

    void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera, Entity selected_entity) {
        // 3D pass first — opaque meshes write depth so 2D sprites overlay correctly.
        Renderer3D::BeginScene(camera);
        GatherAndUploadLights(this, mRegistry);
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
                                 text_comp.Kerning, text_comp.LineSpacing, (int)entt::to_entity(entity));
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
            Renderer2D::DrawQuad(billboard, mCameraIcon, glm::vec4(1.0f), 1.0f, (int)entt::to_entity(entity));
        }

        if (selected_entity && selected_entity.HasComponent<CameraComponent>()) {
            DrawCameraFrustum(GetWorldTransform(selected_entity), selected_entity.GetComponent<CameraComponent>());
        }

        RenderPhysicsColliders();

        Renderer2D::EndScene();
    }

    void Scene::OnRuntimeStart() {
        mRegistry.view<AnimationComponent>().each([](AnimationComponent& anim) {
            anim.CurrentFrame = 0;
            anim.ElapsedTime  = 0.0f;
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
                if (bc2d.IsSensor) {
                    shape_def.isSensor           = true;
                    shape_def.enableSensorEvents = true;
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
                if (cc2d.IsSensor) {
                    shape_def.isSensor           = true;
                    shape_def.enableSensorEvents = true;
                } else {
                    shape_def.enableContactEvents = true;
                }

                b2Circle circle;
                circle.center = { cc2d.Offset.x, cc2d.Offset.y };
                circle.radius = cc2d.Radius * transform.Scale.x;

                cc2d.RuntimeFixture = b2CreateCircleShape(rb2d.RuntimeBody, &shape_def, &circle);
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
            int32_t sub_step_count = 8;
            b2World_Step(mPhysicsWorld, ts, sub_step_count);

            // Resolve a Box2D body back to an Entity via the stored userData handle.
            auto resolve_entity = [this](b2BodyId body_id) -> Entity {
                void* userdata = b2Body_GetUserData(body_id);
                auto  raw      = static_cast<entt::id_type>(reinterpret_cast<uintptr_t>(userdata));
                return { static_cast<entt::entity>(raw), this };
            };

            b2ContactEvents contact_events = b2World_GetContactEvents(mPhysicsWorld);
            for (int i = 0; i < contact_events.beginCount; ++i) {
                Entity a = resolve_entity(b2Shape_GetBody(contact_events.beginEvents[i].shapeIdA));
                Entity b = resolve_entity(b2Shape_GetBody(contact_events.beginEvents[i].shapeIdB));
                ScriptingEngine::OnCollisionBegin(a, b);
            }
            for (int i = 0; i < contact_events.endCount; ++i) {
                Entity a = resolve_entity(b2Shape_GetBody(contact_events.endEvents[i].shapeIdA));
                Entity b = resolve_entity(b2Shape_GetBody(contact_events.endEvents[i].shapeIdB));
                ScriptingEngine::OnCollisionEnd(a, b);
            }

            b2SensorEvents sensor_events = b2World_GetSensorEvents(mPhysicsWorld);
            for (int i = 0; i < sensor_events.beginCount; ++i) {
                Entity a = resolve_entity(b2Shape_GetBody(sensor_events.beginEvents[i].sensorShapeId));
                Entity b = resolve_entity(b2Shape_GetBody(sensor_events.beginEvents[i].visitorShapeId));
                ScriptingEngine::OnSensorBegin(a, b);
            }
            for (int i = 0; i < sensor_events.endCount; ++i) {
                Entity a = resolve_entity(b2Shape_GetBody(sensor_events.endEvents[i].sensorShapeId));
                Entity b = resolve_entity(b2Shape_GetBody(sensor_events.endEvents[i].visitorShapeId));
                ScriptingEngine::OnSensorEnd(a, b);
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

        // 3. Advance sprite animations
        mRegistry.view<AnimationComponent>().each([&](AnimationComponent& anim) {
            if (!anim.IsPlaying || anim.Frames.empty()) return;
            anim.ElapsedTime += ts;
            while (anim.ElapsedTime >= anim.FrameDuration) {
                anim.ElapsedTime -= anim.FrameDuration;
                anim.CurrentFrame++;
                if (anim.CurrentFrame >= (int)anim.Frames.size()) {
                    if (anim.Loop) anim.CurrentFrame = 0;
                    else { anim.CurrentFrame = (int)anim.Frames.size() - 1; anim.IsPlaying = false; }
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
            // 3D pass first — opaque meshes write depth so 2D sprites overlay correctly.
            Renderer3D::BeginScene(*main_camera, camera_transform);
            GatherAndUploadLights(this, mRegistry);
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
                                     text_comp.Kerning, text_comp.LineSpacing, (int)entt::to_entity(entity));
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
            anim.CurrentFrame = 0;
            anim.ElapsedTime  = 0.0f;
            anim.IsPlaying    = true;
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

    void Scene::RenderPhysicsColliders() {
        if (!mShowPhysicsColliders) return;

        glm::vec4 collider_color = { 0.1f, 0.9f, 0.1f, 1.0f };

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

            int entity_id = (int)entt::to_entity(entity);
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

            Renderer2D::DrawCircle(transform_mat, collider_color, 0.05f, 0.005f, (int)entt::to_entity(entity));
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
