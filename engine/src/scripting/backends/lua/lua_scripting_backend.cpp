#include "scripting/backends/lua/lua_scripting_backend.h"
#include "loom/audio/audio_engine.h"
#include "loom/scene/components.h"
#include "loom/scene/entity.h"
#include "loom/scene/scene.h"
#include "loom/scene/scene_serializer.h"
#include "loom/core/input.h"
#include "loom/core/key_codes.h"
#include "loom/core/mouse_codes.h"
#include "loom/core/log.h"
#include "loom/project/project.h"
#include "loom/scene/scene_loader.h"
#include <box2d/box2d.h>
#include <glm/glm.hpp>
#include <filesystem>

namespace Loom {

// TU-local entity wrapper exposing ECS transform/tag accessors to Lua.
namespace {

    struct LuaEntityWrapper {
        Entity  handle;
        Scene*  scene = nullptr;

        glm::vec3   GetTranslation()            { return handle.GetComponent<TransformComponent>().Translation; }
        void        SetTranslation(glm::vec3 v) { handle.GetComponent<TransformComponent>().Translation = v; }
        glm::vec3   GetRotation()               { return handle.GetComponent<TransformComponent>().Rotation; }
        void        SetRotation(glm::vec3 v)    { handle.GetComponent<TransformComponent>().Rotation = v; }
        glm::vec3   GetScale()                  { return handle.GetComponent<TransformComponent>().Scale; }
        void        SetScale(glm::vec3 v)       { handle.GetComponent<TransformComponent>().Scale = v; }
        std::string GetTag()                    { return handle.GetComponent<TagComponent>().Tag; }

        LuaEntityWrapper FindByTag(const std::string& tag) {
            if (!scene) return {};
            return LuaEntityWrapper{ scene->GetEntityByTag(tag), scene };
        }

        LuaEntityWrapper Spawn() {
            if (!scene) return {};
            return LuaEntityWrapper{ scene->CreateEntity("New Entity"), scene };
        }

        void Destroy() {
            if (scene && handle) scene->DestroyEntity(handle);
            handle = {};
            scene  = nullptr;
        }

        LuaEntityWrapper Instantiate(const std::string& prefab_path) {
            if (!scene) {
                LOOM_CORE_ERROR("[Lua] Instantiate: no active scene");
                return {};
            }
            auto full = Project::GetAssetFileSystemPath(prefab_path).generic_string();
            Entity spawned = SceneSerializer::DeserializePrefabInto(full, scene);
            return LuaEntityWrapper{ spawned, scene };
        }

        // --- Audio API ---

        void PlayAudio() {
            if (!handle.HasComponent<AudioSourceComponent>()) return;
            auto& asc = handle.GetComponent<AudioSourceComponent>();
            if (asc.AssetPath.empty()) return;
            auto full = Project::GetAssetFileSystemPath(asc.AssetPath).generic_string();
            AudioEngine::PlaySource(asc, full);
        }

        void StopAudio() {
            if (!handle.HasComponent<AudioSourceComponent>()) return;
            AudioEngine::StopSource(handle.GetComponent<AudioSourceComponent>());
        }

        bool IsAudioPlaying() {
            if (!handle.HasComponent<AudioSourceComponent>()) return false;
            return AudioEngine::IsPlaying(handle.GetComponent<AudioSourceComponent>());
        }

        void SetVolume(float v) {
            if (!handle.HasComponent<AudioSourceComponent>()) return;
            AudioEngine::SetVolume(handle.GetComponent<AudioSourceComponent>(), v);
        }

        void SetPitch(float p) {
            if (!handle.HasComponent<AudioSourceComponent>()) return;
            AudioEngine::SetPitch(handle.GetComponent<AudioSourceComponent>(), p);
        }

        // --- Physics API ---

        void SetLinearVelocity(glm::vec2 v) {
            if (!handle.HasComponent<Rigidbody2DComponent>()) return;
            auto& rb = handle.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.RuntimeBody)) return;
            b2Body_SetLinearVelocity(rb.RuntimeBody, { v.x, v.y });
        }

        glm::vec2 GetLinearVelocity() {
            if (!handle.HasComponent<Rigidbody2DComponent>()) return {};
            auto& rb = handle.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.RuntimeBody)) return {};
            b2Vec2 vel = b2Body_GetLinearVelocity(rb.RuntimeBody);
            return { vel.x, vel.y };
        }

        void ApplyForce(glm::vec2 v) {
            if (!handle.HasComponent<Rigidbody2DComponent>()) return;
            auto& rb = handle.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.RuntimeBody)) return;
            b2Body_ApplyForceToCenter(rb.RuntimeBody, { v.x, v.y }, true);
        }

        void ApplyImpulse(glm::vec2 v) {
            if (!handle.HasComponent<Rigidbody2DComponent>()) return;
            auto& rb = handle.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.RuntimeBody)) return;
            b2Body_ApplyLinearImpulseToCenter(rb.RuntimeBody, { v.x, v.y }, true);
        }
    };

} // anonymous namespace

    LuaScriptingBackend::LuaScriptingBackend() {
        mLua.open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table
        );
        BindLuaAPI();
    }

    void LuaScriptingBackend::BindLuaAPI() {
        mLua.new_usertype<glm::vec2>("Vec2",
            sol::call_constructor,
            sol::constructors<glm::vec2(), glm::vec2(float, float)>(),
            "x", &glm::vec2::x,
            "y", &glm::vec2::y,
            sol::meta_function::addition,
                [](const glm::vec2& a, const glm::vec2& b) { return a + b; },
            sol::meta_function::subtraction,
                [](const glm::vec2& a, const glm::vec2& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const glm::vec2& v, float s) { return v * s; },
                [](float s, const glm::vec2& v) { return s * v; }
            ),
            sol::meta_function::to_string,
                [](const glm::vec2& v) {
                    return "Vec2(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
                }
        );

        mLua.new_usertype<glm::vec3>("Vec3",
            sol::call_constructor,
            sol::constructors<glm::vec3(), glm::vec3(float, float, float)>(),
            "x", &glm::vec3::x,
            "y", &glm::vec3::y,
            "z", &glm::vec3::z,
            sol::meta_function::addition,
                [](const glm::vec3& a, const glm::vec3& b) { return a + b; },
            sol::meta_function::subtraction,
                [](const glm::vec3& a, const glm::vec3& b) { return a - b; },
            sol::meta_function::multiplication, sol::overload(
                [](const glm::vec3& v, float s) { return v * s; },
                [](float s, const glm::vec3& v) { return s * v; }
            ),
            sol::meta_function::to_string,
                [](const glm::vec3& v) {
                    return "Vec3(" + std::to_string(v.x) + ", "
                                   + std::to_string(v.y) + ", "
                                   + std::to_string(v.z) + ")";
                }
        );

        mLua.new_usertype<LuaEntityWrapper>("Entity",
            "GetTranslation",     &LuaEntityWrapper::GetTranslation,
            "SetTranslation",     &LuaEntityWrapper::SetTranslation,
            "GetRotation",        &LuaEntityWrapper::GetRotation,
            "SetRotation",        &LuaEntityWrapper::SetRotation,
            "GetScale",           &LuaEntityWrapper::GetScale,
            "SetScale",           &LuaEntityWrapper::SetScale,
            "GetTag",             &LuaEntityWrapper::GetTag,
            "FindByTag",          &LuaEntityWrapper::FindByTag,
            "Spawn",              &LuaEntityWrapper::Spawn,
            "Destroy",            &LuaEntityWrapper::Destroy,
            "Instantiate",        &LuaEntityWrapper::Instantiate,
            // Audio
            "PlayAudio",          &LuaEntityWrapper::PlayAudio,
            "StopAudio",          &LuaEntityWrapper::StopAudio,
            "IsAudioPlaying",     &LuaEntityWrapper::IsAudioPlaying,
            "SetVolume",          &LuaEntityWrapper::SetVolume,
            "SetPitch",           &LuaEntityWrapper::SetPitch,
            // Physics
            "SetLinearVelocity",  &LuaEntityWrapper::SetLinearVelocity,
            "GetLinearVelocity",  &LuaEntityWrapper::GetLinearVelocity,
            "ApplyForce",         &LuaEntityWrapper::ApplyForce,
            "ApplyImpulse",       &LuaEntityWrapper::ApplyImpulse
        );

        sol::table input = mLua.create_named_table("Input");
        input.set_function("IsKeyPressed",
            [](int k) { return Input::IsKeyPressed(static_cast<Key>(k)); });
        input.set_function("IsMouseButtonPressed",
            [](int b) { return Input::IsMouseButtonPressed(static_cast<Mouse>(b)); });
        input.set_function("GetMouseX", []() { return Input::GetMouseX(); });
        input.set_function("GetMouseY", []() { return Input::GetMouseY(); });

        sol::table key = mLua.create_named_table("Key");
        key["Space"]        = (int)Key::Space;
        key["Escape"]       = (int)Key::Escape;
        key["Enter"]        = (int)Key::Enter;
        key["Tab"]          = (int)Key::Tab;
        key["Backspace"]    = (int)Key::Backspace;
        key["Up"]           = (int)Key::Up;
        key["Down"]         = (int)Key::Down;
        key["Left"]         = (int)Key::Left;
        key["Right"]        = (int)Key::Right;
        key["LeftShift"]    = (int)Key::LeftShift;
        key["RightShift"]   = (int)Key::RightShift;
        key["LeftControl"]  = (int)Key::LeftControl;
        key["RightControl"] = (int)Key::RightControl;
        key["LeftAlt"]      = (int)Key::LeftAlt;
        key["RightAlt"]     = (int)Key::RightAlt;
        key["A"] = (int)Key::A;  key["B"] = (int)Key::B;  key["C"] = (int)Key::C;
        key["D"] = (int)Key::D;  key["E"] = (int)Key::E;  key["F"] = (int)Key::F;
        key["G"] = (int)Key::G;  key["H"] = (int)Key::H;  key["I"] = (int)Key::I;
        key["J"] = (int)Key::J;  key["K"] = (int)Key::K;  key["L"] = (int)Key::L;
        key["M"] = (int)Key::M;  key["N"] = (int)Key::N;  key["O"] = (int)Key::O;
        key["P"] = (int)Key::P;  key["Q"] = (int)Key::Q;  key["R"] = (int)Key::R;
        key["S"] = (int)Key::S;  key["T"] = (int)Key::T;  key["U"] = (int)Key::U;
        key["V"] = (int)Key::V;  key["W"] = (int)Key::W;  key["X"] = (int)Key::X;
        key["Y"] = (int)Key::Y;  key["Z"] = (int)Key::Z;
        key["D0"] = (int)Key::D0; key["D1"] = (int)Key::D1; key["D2"] = (int)Key::D2;
        key["D3"] = (int)Key::D3; key["D4"] = (int)Key::D4; key["D5"] = (int)Key::D5;
        key["D6"] = (int)Key::D6; key["D7"] = (int)Key::D7; key["D8"] = (int)Key::D8;
        key["D9"] = (int)Key::D9;
        key["F1"]  = (int)Key::F1;  key["F2"]  = (int)Key::F2;  key["F3"]  = (int)Key::F3;
        key["F4"]  = (int)Key::F4;  key["F5"]  = (int)Key::F5;  key["F6"]  = (int)Key::F6;
        key["F7"]  = (int)Key::F7;  key["F8"]  = (int)Key::F8;  key["F9"]  = (int)Key::F9;
        key["F10"] = (int)Key::F10; key["F11"] = (int)Key::F11; key["F12"] = (int)Key::F12;

        sol::table mouse = mLua.create_named_table("Mouse");
        mouse["Left"]    = (int)Mouse::ButtonLeft;
        mouse["Right"]   = (int)Mouse::ButtonRight;
        mouse["Middle"]  = (int)Mouse::ButtonMiddle;
        mouse["Button0"] = (int)Mouse::Button0;
        mouse["Button1"] = (int)Mouse::Button1;
        mouse["Button2"] = (int)Mouse::Button2;

        auto lua_args_to_string = [](sol::variadic_args args) -> std::string {
            std::string out;
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) out += '\t';
                const sol::object& v = args[i];
                switch (v.get_type()) {
                    case sol::type::string:  out += v.as<std::string>(); break;
                    case sol::type::number:  {
                        double d = v.as<double>();
                        out += (d == std::floor(d))
                            ? std::to_string(static_cast<long long>(d))
                            : std::to_string(d);
                        break;
                    }
                    case sol::type::boolean: out += v.as<bool>() ? "true" : "false"; break;
                    case sol::type::nil:     out += "nil"; break;
                    default:                 out += "(object)"; break;
                }
            }
            return out;
        };

        sol::table log = mLua.create_named_table("Log");
        log.set_function("Trace", [lua_args_to_string](sol::variadic_args args) { LOOM_CORE_TRACE("[Lua] {}", lua_args_to_string(args)); });
        log.set_function("Info",  [lua_args_to_string](sol::variadic_args args) { LOOM_CORE_INFO("[Lua] {}",  lua_args_to_string(args)); });
        log.set_function("Warn",  [lua_args_to_string](sol::variadic_args args) { LOOM_CORE_WARN("[Lua] {}",  lua_args_to_string(args)); });
        log.set_function("Error", [lua_args_to_string](sol::variadic_args args) { LOOM_CORE_ERROR("[Lua] {}", lua_args_to_string(args)); });

        sol::table physics = mLua.create_named_table("Physics");
        physics.set_function("Raycast", [this](glm::vec3 origin, glm::vec3 direction, float distance) -> sol::table {
            sol::table result = mLua.create_table();
            result["hit"] = false;
            if (!mActiveScene) return result;

            auto hit = mActiveScene->Raycast2D({ origin.x, origin.y }, { direction.x, direction.y }, distance);
            result["hit"]    = hit.hit;
            result["point"]  = glm::vec3(hit.point,  0.0f);
            result["normal"] = glm::vec3(hit.normal, 0.0f);
            if (hit.hit && hit.entityHandle != entt::null)
                result["entity"] = LuaEntityWrapper{ Entity{ hit.entityHandle, mActiveScene }, mActiveScene };
            return result;
        });

        physics.set_function("OverlapCircle", [this](glm::vec2 center, float radius) -> sol::table {
            sol::table result = mLua.create_table();
            if (!mActiveScene) return result;
            auto entities = mActiveScene->OverlapCircle2D(center, radius);
            int idx = 1;
            for (auto e : entities) {
                if (e != entt::null)
                    result[idx++] = LuaEntityWrapper{ Entity{ e, mActiveScene }, mActiveScene };
            }
            return result;
        });

        physics.set_function("OverlapBox", [this](glm::vec2 center, glm::vec2 half_extents) -> sol::table {
            sol::table result = mLua.create_table();
            if (!mActiveScene) return result;
            auto entities = mActiveScene->OverlapBox2D(center, half_extents);
            int idx = 1;
            for (auto e : entities) {
                if (e != entt::null)
                    result[idx++] = LuaEntityWrapper{ Entity{ e, mActiveScene }, mActiveScene };
            }
            return result;
        });

        sol::table scene = mLua.create_named_table("Scene");
        scene.set_function("Load", [](const std::string& relative_path) {
            SceneLoader::Get().QueueLoad(relative_path);
        });
        scene.set_function("Reload", []() {
            SceneLoader::Get().QueueReload();
        });
    }

    void LuaScriptingBackend::OnRuntimeStart(Scene* scene) {
        mActiveScene = scene;
        mFileWatcher = std::make_unique<FileWatcher>();

        auto view = scene->GetAllEntitiesWith<LuaScriptComponent>();
        for (auto entity_id : view) {
            LoadEntityScript(entity_id, scene);

            Entity entity = { entity_id, scene };
            auto& lsc = entity.GetComponent<LuaScriptComponent>();
            std::string full_path = Project::GetAssetFileSystemPath(lsc.ScriptPath).generic_string();
            mFileWatcher->Watch(full_path);
        }
    }

    void LuaScriptingBackend::OnRuntimeUpdate(Timestep ts, Scene* scene) {
        if (mFileWatcher) {
            for (const auto& path : mFileWatcher->FlushChanges())
                OnFileChanged(path);
        }

        auto view = scene->GetAllEntitiesWith<LuaScriptComponent>();
        for (auto entity_id : view) {
            auto it = mScriptInstances.find(entity_id);
            if (it == mScriptInstances.end())
                continue;

            sol::protected_function on_update = it->second["OnUpdate"];
            if (!on_update.valid())
                continue;

            auto res = on_update(static_cast<float>(ts));
            if (!res.valid()) {
                sol::error err = res;
                Entity entity = { entity_id, scene };
                LOOM_CORE_ERROR("Lua OnUpdate error in '{}': {}",
                    entity.GetComponent<LuaScriptComponent>().ScriptPath, err.what());
            }
        }
    }

    void LuaScriptingBackend::OnRuntimeStop() {
        mFileWatcher.reset();

        for (auto& [entity_id, env] : mScriptInstances) {
            sol::protected_function on_destroy = env["OnDestroy"];
            if (on_destroy.valid()) {
                auto res = on_destroy();
                if (!res.valid()) {
                    sol::error err = res;
                    LOOM_CORE_ERROR("Lua OnDestroy error: {}", err.what());
                }
            }
        }

        mScriptInstances.clear();
        mActiveScene = nullptr;
    }

    void LuaScriptingBackend::DispatchCollisionEvent(entt::entity self, entt::entity other, const char* fn_name) {
        auto it = mScriptInstances.find(self);
        if (it == mScriptInstances.end()) return;

        sol::protected_function fn = it->second[fn_name];
        if (!fn.valid()) return;

        LuaEntityWrapper other_wrapper{ Entity{ other, mActiveScene }, mActiveScene };
        auto res = fn(other_wrapper);
        if (!res.valid()) {
            sol::error err = res;
            Entity entity = { self, mActiveScene };
            LOOM_CORE_ERROR("Lua {} error in '{}': {}", fn_name,
                entity.GetComponent<LuaScriptComponent>().ScriptPath, err.what());
        }
    }

    void LuaScriptingBackend::OnCollisionBegin(entt::entity a, entt::entity b) {
        if (!mActiveScene) return;
        DispatchCollisionEvent(a, b, "OnCollisionBegin");
        DispatchCollisionEvent(b, a, "OnCollisionBegin");
    }

    void LuaScriptingBackend::OnCollisionEnd(entt::entity a, entt::entity b) {
        if (!mActiveScene) return;
        DispatchCollisionEvent(a, b, "OnCollisionEnd");
        DispatchCollisionEvent(b, a, "OnCollisionEnd");
    }

    void LuaScriptingBackend::OnSensorBegin(entt::entity a, entt::entity b) {
        if (!mActiveScene) return;
        DispatchCollisionEvent(a, b, "OnSensorBegin");
        DispatchCollisionEvent(b, a, "OnSensorBegin");
    }

    void LuaScriptingBackend::OnSensorEnd(entt::entity a, entt::entity b) {
        if (!mActiveScene) return;
        DispatchCollisionEvent(a, b, "OnSensorEnd");
        DispatchCollisionEvent(b, a, "OnSensorEnd");
    }

    void LuaScriptingBackend::OnFileChanged(const std::string& path) {
        if (!mActiveScene) return;

        auto view = mActiveScene->GetAllEntitiesWith<LuaScriptComponent>();
        for (auto entity_id : view) {
            Entity entity = { entity_id, mActiveScene };
            auto& lsc = entity.GetComponent<LuaScriptComponent>();
            std::string full_path = Project::GetAssetFileSystemPath(lsc.ScriptPath).generic_string();

            if (full_path == path || lsc.ScriptPath == path) {
                UnloadEntityScript(entity_id);
                LoadEntityScript(entity_id, mActiveScene);
                LOOM_CORE_INFO("Hot-reloaded Lua script: {}", lsc.ScriptPath);
            }
        }
    }

    void LuaScriptingBackend::LoadEntityScript(entt::entity entity_id, Scene* scene) {
        Entity entity = { entity_id, scene };
        auto& lsc = entity.GetComponent<LuaScriptComponent>();

        std::filesystem::path asset_dir = Project::GetAssetDirectory();
        std::filesystem::path script_path(lsc.ScriptPath);

        std::string full_path = (asset_dir / script_path).generic_string();
        LOOM_CORE_INFO("Loading Lua script '{}' -> '{}'", lsc.ScriptPath, full_path);

        sol::environment env(mLua, sol::create, mLua.globals());
        env["entity"] = LuaEntityWrapper{ entity, scene };

        auto result = mLua.safe_script_file(full_path, env, sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            LOOM_CORE_ERROR("Lua script load error '{}': {}", full_path, err.what());
            return;
        }

        mScriptInstances[entity_id] = std::move(env);

        sol::protected_function on_create = mScriptInstances[entity_id]["OnCreate"];
        if (on_create.valid()) {
            auto res = on_create();
            if (!res.valid()) {
                sol::error err = res;
                LOOM_CORE_ERROR("Lua OnCreate error in '{}': {}", lsc.ScriptPath, err.what());
            }
        }
    }

    void LuaScriptingBackend::UnloadEntityScript(entt::entity entity_id) {
        auto it = mScriptInstances.find(entity_id);
        if (it == mScriptInstances.end()) return;

        sol::protected_function on_destroy = it->second["OnDestroy"];
        if (on_destroy.valid()) {
            auto res = on_destroy();
            if (!res.valid()) {
                sol::error err = res;
                LOOM_CORE_ERROR("Lua OnDestroy error: {}", err.what());
            }
        }

        mScriptInstances.erase(it);
    }

} // namespace Loom
