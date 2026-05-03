#include "scripting/backends/lua/lua_scripting_backend.h"
#include "loom/scene/components.h"
#include "loom/scene/entity.h"
#include "loom/scene/scene.h"
#include "loom/core/input.h"
#include "loom/core/key_codes.h"
#include "loom/core/mouse_codes.h"
#include "loom/core/log.h"
#include "loom/project/project.h"
#include <glm/glm.hpp>
#include <filesystem>

namespace Loom {

// ---------------------------------------------------------------------------
// LuaEntityWrapper — exposes a scoped set of ECS accessors to Lua scripts.
// Defined here (TU-local) so sol2 can register it without polluting public API.
// ---------------------------------------------------------------------------
namespace {

struct LuaEntityWrapper {
    Entity handle;

    glm::vec3   GetTranslation()       { return handle.GetComponent<TransformComponent>().Translation; }
    void        SetTranslation(glm::vec3 v) { handle.GetComponent<TransformComponent>().Translation = v; }
    glm::vec3   GetRotation()          { return handle.GetComponent<TransformComponent>().Rotation; }
    void        SetRotation(glm::vec3 v)    { handle.GetComponent<TransformComponent>().Rotation = v; }
    glm::vec3   GetScale()             { return handle.GetComponent<TransformComponent>().Scale; }
    void        SetScale(glm::vec3 v)       { handle.GetComponent<TransformComponent>().Scale = v; }
    std::string GetTag()               { return handle.GetComponent<TagComponent>().Tag; }
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction & API binding
// ---------------------------------------------------------------------------

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
    // ----- Vec3 -----
    mLua.new_usertype<glm::vec3>("Vec3",
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

    // ----- Entity -----
    mLua.new_usertype<LuaEntityWrapper>("Entity",
        "GetTranslation", &LuaEntityWrapper::GetTranslation,
        "SetTranslation", &LuaEntityWrapper::SetTranslation,
        "GetRotation",    &LuaEntityWrapper::GetRotation,
        "SetRotation",    &LuaEntityWrapper::SetRotation,
        "GetScale",       &LuaEntityWrapper::GetScale,
        "SetScale",       &LuaEntityWrapper::SetScale,
        "GetTag",         &LuaEntityWrapper::GetTag
    );

    // ----- Input -----
    sol::table input = mLua.create_named_table("Input");
    input.set_function("IsKeyPressed",
        [](int k) { return Input::IsKeyPressed(static_cast<Key>(k)); });
    input.set_function("IsMouseButtonPressed",
        [](int b) { return Input::IsMouseButtonPressed(static_cast<Mouse>(b)); });
    input.set_function("GetMouseX", []() { return Input::GetMouseX(); });
    input.set_function("GetMouseY", []() { return Input::GetMouseY(); });

    // ----- Key constants -----
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

    // ----- Mouse constants -----
    sol::table mouse = mLua.create_named_table("Mouse");
    mouse["Left"]    = (int)Mouse::ButtonLeft;
    mouse["Right"]   = (int)Mouse::ButtonRight;
    mouse["Middle"]  = (int)Mouse::ButtonMiddle;
    mouse["Button0"] = (int)Mouse::Button0;
    mouse["Button1"] = (int)Mouse::Button1;
    mouse["Button2"] = (int)Mouse::Button2;

    // ----- Log -----
    sol::table log = mLua.create_named_table("Log");
    log.set_function("Trace", [](const std::string& msg) { LOOM_CORE_TRACE("[Lua] {}", msg); });
    log.set_function("Info",  [](const std::string& msg) { LOOM_CORE_INFO("[Lua] {}",  msg); });
    log.set_function("Warn",  [](const std::string& msg) { LOOM_CORE_WARN("[Lua] {}",  msg); });
    log.set_function("Error", [](const std::string& msg) { LOOM_CORE_ERROR("[Lua] {}", msg); });
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void LuaScriptingBackend::OnRuntimeStart(Scene* scene) {
    mActiveScene = scene;

    auto view = scene->GetAllEntitiesWith<LuaScriptComponent>();
    for (auto entity_id : view)
        LoadEntityScript(entity_id, scene);
}

void LuaScriptingBackend::OnRuntimeUpdate(Timestep ts, Scene* scene) {
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

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void LuaScriptingBackend::LoadEntityScript(entt::entity entity_id, Scene* scene) {
    Entity entity = { entity_id, scene };
    auto& lsc = entity.GetComponent<LuaScriptComponent>();

    std::filesystem::path asset_dir = Project::GetAssetDirectory();
    std::filesystem::path script_path(lsc.ScriptPath);

    std::string full_path = (asset_dir / script_path).generic_string();
    LOOM_CORE_INFO("Loading Lua script '{}' -> '{}'", lsc.ScriptPath, full_path);

    sol::environment env(mLua, sol::create, mLua.globals());
    env["entity"] = LuaEntityWrapper{ entity };

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
