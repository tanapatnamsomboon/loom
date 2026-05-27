You are an expert C++ Game Engine Developer assisting with the development of "Loom Engine".

Before modifying or adding code, review the architecture and conventions below to ensure all suggestions align with the existing codebase.

# 0. Terminology & Context

To ensure accurate communication and architectural decisions, we strictly separate roles and domains. Whenever an instruction is given, evaluate which domain and persona it applies to before acting.

## The 2 Domains (What are we building?)

1. **The Engine (Loom Engine):** The core C++ codebase — OpenGL renderer, ECS, Lua bindings, and Editor UI (`engine/`, `weaver/`). Example: "Optimize compile time" → make the C++ Engine build faster in CMake/Visual Studio.
2. **The Game (Project/Assets):** The specific game project created *using* the Engine — the `.loomproj`, scenes, assets, and Lua scripts living in an asset directory. Example: "Add a jump mechanic" → write a Lua script or scene asset, not C++ engine code.

## The 3 Personas (Who are we talking about?)

1. **Engine Developer (Us):** You and I. We write C++ to build the Engine. "I need to fix a bug" → Engine Developer speaking.
2. **Game Developer (The User):** The person using Weaver and writing Lua scripts to create a Game. "Improve user experience" or "make it easier to use" → make the Engine's tooling better *for* the Game Developer.
3. **The Player (End-User):** The person playing the final compiled Game via WeaverRuntime. "Gameplay performance" or "frame drops" → the Player's experience at runtime.

All architectural suggestions, feature designs, and bug analyses must be framed using these terms.

---

# 1. Project Overview

- **Name:** Loom Engine
- **Language:** C++ (Standard: C++20)
- **Build System:** CMake (configured via `CMakeLists.txt` and `CMakePresets.json`)
- **Graphics API:** OpenGL (wrapped inside a custom, abstract Renderer API)
- **Namespace:** All engine types live in the `Loom::` namespace. All editor types live in `Weaver::`.

---

# 2. Tech Stack & Vendor Libraries

Third-party libraries are located in the `vendor/` directory. Always use these instead of rolling your own:

| Purpose                       | Library              | Vendor Dir          |
|-------------------------------|----------------------|---------------------|
| ECS (Entity Component System) | EnTT                 | `entt`              |
| Windowing & Input             | GLFW                 | `glfw`              |
| OpenGL Loader                 | GLAD                 | `glad`              |
| UI / Editor                   | Dear ImGui           | `imgui`             |
| Math                          | GLM                  | `glm`               |
| Logging                       | spdlog               | `spdlog`            |
| Image Loading                 | stb_image            | `stb`               |
| Physics (2D)                  | Box2D                | `box2d`             |
| Serialization                 | YAML-CPP             | `yaml-cpp`          |
| File Dialogs                  | ImGuiFileDialog      | `imguifiledialog`   |
| Transform Gizmo               | ImGuizmo             | `imguizmo`          |
| Scripting VM                  | Lua 5.4              | `lua`               |
| Lua C++ Bindings              | sol2 v3.5.0          | `sol2`              |
| Mesh Loading (GLTF/GLB)       | cgltf                | `cgltf`             |
| Physics (3D)                  | Jolt Physics         | `jolt`              |

---

# 3. Directory Structure Architecture

## `engine/` — Core Engine Library

The engine compiles to a static/dynamic library. Internal headers are exposed under the `<loom/...>` include prefix (e.g., `#include <loom/scene/scene.h>`).

- `core/`: Application loop, LayerStack, Events system, Input, Window abstraction, Timestep, UUID, and core macros (`LOOM_BIND_EVENT_FN`, `LOOM_CORE_*` log macros).
- `renderer/`: Abstract Renderer API, Shaders, Textures, Buffers, Framebuffers (RGBA16F for HDR scene, DEPTH24STENCIL8 + DEPTH32F for shadow maps — size and max distance live on `Project::Config::Graphics` (defaults: 4096 per cascade, ~256 MB shadow VRAM total; 200u max distance), pushed into `Renderer3D` at `Init` and on every project open via `Renderer3D::SetShadowMapSize` / `SetShadowMaxDistance`), VertexArray (multi-VBO supported: attribute index accumulates across `AddVertexBuffer` calls so skinned VAOs can attach a parallel joints+weights buffer at slots 4/5), Cameras (`OrthographicCamera`, `EditorCamera`), `Renderer2D` (writes linearized values into the HDR framebuffer — `quad.frag` / `circle.frag` / `line.frag` / `grid.frag` apply `pow(rgb, 2.2)` at output), `Renderer3D` (mesh facade — PBR via Cook-Torrance direct lighting + Karis split-sum IBL, raw linear HDR output, 4-cascade CSM with 5×5 PCF, scene-owned environment with editor fallback; `Submit` + `SubmitShadow` auto-branch on `MeshAsset::IsSkinned()` between `mesh.{vert,frag}` / `shadow_depth.{vert,frag}` for static meshes and `mesh_skinned.{vert,frag}` / `shadow_depth_skinned.{vert,frag}` for skinned, walking the skeleton and uploading skin matrices to the Bones UBO at binding=1 per draw; both accept optional `sampled_local_transforms` so an animator can override `LocalBind` per joint, null falls back to bind pose; owns the unified post pipeline **Bloom → ACES Tonemap → FXAA**, `DrawSkybox`, and a one-time 512² RG16F BRDF LUT generated at `Init`), `TextureCubemap` (`CreateFromEquirect` / `CreateIrradiance` / `CreatePrefiltered`), `MeshAsset` (GLTF/GLB import via cgltf, with embedded-texture extraction via `ExtractEmbeddedTextures` for `.glb` buffer-view and base64 data-URI images; carries a `Skeleton` for skinned meshes — parent indices, IBM, bind-pose local TRS + bind T/R/S components for partial-channel animation fallback, `RootWorld` for the non-joint ancestor chain above root joints — plus a list of `AnimationClip3D` parsed from glTF animations), `FontAsset` (TTF glyph atlas via stb_truetype).
- `scene/`: ECS implementation. Contains `scene.cpp`, `entity.cpp`, `components.h` (all component structs including `LuaScriptComponent`, the 2D `AnimationComponent` for sprite frames, and `SkeletalAnimationComponent` for 3D rigs — clip name, time, speed, loop, playing + per-frame sampled-locals scratch), `scene_serializer`, and `script_registry`. `Scene` also owns the skybox HDR path + lazily-built env/irradiance/prefilter cubemaps as a scene property (persisted via `SceneSerializer`, propagated through `Scene::Copy`). `OnUpdateEditor` accepts optional `fallback_irradiance` + `fallback_prefilter` so the editor can paint a default IBL onto envless scenes for build-time UX; `OnUpdateRuntime` renders only the scene's own env with no fallback (deliberate — empty env = dark ambient in the shipped game). Both update paths run `SampleSkeletalAnimations` before the mesh draw — sampling TRS keyframes per joint (LERP/STEP/CubicSpline-as-LERP, SLERP for rotation), composing into a local matrix array, handed to `Renderer3D::Submit` / `SubmitShadow` to override `LocalBind` during the skeleton walk.
- `physics/`: `PhysicsEngine3D` singleton — process-wide Jolt init (default allocator/factory/types, `JPH::TempAllocatorImpl`, `JobSystemThreadPool`, shared layer-filter interfaces). The per-scene `JPH::PhysicsSystem` lives on `Scene`, not here.
- `asset/`: `AssetManager` (centralized loader/cache for shaders, textures, fonts, and meshes) and `FontManager` (UI font atlas, exposes `FontType` slots via static `Get`/`Push`/`Pop`). All loaders (`GetTexture`, `GetShader`, `GetFont`, `GetMesh`) take paths **verbatim** — they do not resolve relative paths themselves. The caller is responsible for converting a project-relative path to an absolute filesystem path via the `Project::` helpers below before invoking the loader.
- `project/`: `Project` and `ProjectSerializer` — manage project config (name, asset directory, start scene, runtime window settings, **`GraphicsConfig` quality knobs** — currently `ShadowMapSize` + `ShadowMaxDistance`, serialized under a `Graphics:` sub-map). **Asset path resolution helpers** (use these instead of hand-rolling `cwd / relative_path`):
  - `Project::GetActive()` → `std::shared_ptr<Project>` for the currently loaded project (may be null — guard before deref).
  - `Project::GetAssetDirectory()` → absolute path to the project's asset root (`<project_dir>/<config.AssetDirectory>`).
  - `Project::GetAssetFileSystemPath(relative_path)` → resolves a project-relative path (e.g., `"models/box.glb"`) into an absolute filesystem path. **This is the canonical way to load any asset stored on a component.**
  - `Project::GetEngineAssetFileSystemPath(relative_path)` → same, but rooted at the engine's `resources/` directory (for shaders, editor icons, default fonts, etc.).
  - Convention: components store project-relative paths (e.g., `SpriteRendererComponent::TexturePath`); the consuming system (scene update, serializer) converts via `Project::GetAssetFileSystemPath` before calling `AssetManager::Get*`.
- `math/`: Engine math utilities (e.g., `Math::DecomposeTransform`).
- `platform/`: Platform-specific implementations (e.g., `platform/opengl/` for OpenGL buffer/shader/texture implementations, `platform/windows/` for input and window).
- `scripting/`: Scripting subsystem.
  - `scripting_engine.h/.cpp` (public): Singleton facade. `Init()` creates the Lua backend; `Shutdown()` tears it down. `OnRuntimeStart/Update/Stop` are forwarded by `Scene`. Initialized automatically by `Application`.
  - `backends/scripting_backend.h` (private): `IScriptingBackend` pure-virtual interface (`OnRuntimeStart`, `OnRuntimeUpdate`, `OnRuntimeStop`, `OnCollisionBegin`, `OnCollisionEnd`, `OnSensorBegin`, `OnSensorEnd`, `OnAnimationEvent`, `OnFileChanged`).
  - `backends/lua/lua_scripting_backend.h/.cpp` (private): Concrete Lua 5.4.4 + sol2 v3.5.0 backend. Manages one `sol::state`, per-entity `sol::environment` instances, and hot-reload via `OnFileChanged`. Binds `Vec2`, `Vec3`, `Entity` (transform/tag/audio/2D-and-3D-physics/animation accessors), `Input`, `Key`, `Mouse`, `Log`, `Physics` (Raycast/OverlapCircle/OverlapBox), and `Scene` (Load/Reload) to Lua.

### Lua Script API (for `LuaScriptComponent` scripts)

Each script runs in an isolated `sol::environment`. The global `entity` is a handle to the owning entity. Authoritative bindings live in `lua_scripting_backend.cpp` — this is just the surface.

```lua
-- Lifecycle (define what you need):
function OnCreate()  end                      function OnUpdate(ts) end
function OnDestroy() end
function OnCollisionBegin(other) end          function OnCollisionEnd(other) end
function OnSensorBegin(other) end             function OnSensorEnd(other) end
function OnAnimationEvent(name) end           -- fires when an AnimationClip frame's tagged event hits

-- Globals: entity, Input, Key, Mouse, Log, Physics, Scene, Vec2, Vec3
-- entity:  GetTranslation/SetTranslation, GetRotation/SetRotation, GetScale/SetScale
--          GetTag, FindByTag(tag), Spawn(), Destroy(), Instantiate(path)
--          PlayAudio/StopAudio/IsAudioPlaying/SetVolume/SetPitch     -- AudioSourceComponent
--          SetLinearVelocity/GetLinearVelocity/ApplyForce/ApplyImpulse        -- Rigidbody2DComponent
--          SetLinearVelocity3D/GetLinearVelocity3D/ApplyForce3D/ApplyImpulse3D -- Rigidbody3DComponent
--          PlayAnimation(name)/StopAnimation()/SetAnimationFrame(n)
--            /GetAnimationFrame()/IsAnimationPlaying()/GetCurrentAnimation()  -- AnimationComponent
-- Input:   IsKeyPressed(Key.W), GetMouseX(), ...
-- Physics: Raycast(origin, dir, dist) -> {hit, point, normal, entity}
--          OverlapCircle(center, radius) | OverlapBox(center, half_extents)
-- Scene:   Load("scenes/x.loom"), Reload()    -- queued, fires end-of-frame
```

## `weaver/` — Editor Application

Built on top of the engine. All editor code is in the `Weaver::` namespace.

- `src/editor_layer.h/.cpp`: Thin orchestrator layer. Owns the `EditorContext`, all panels, and all manager objects. Handles the ImGui dockspace, menu bar, and input routing.
- `src/editor_context.h`: Shared mutable state struct (`EditorContext`) and `GridSettings`. Passed by reference to all panels and managers to avoid tight coupling. Carries cross-cutting state: scene lifecycle (`SceneState`, active scene, history, dirty flag), editor camera, viewport geometry, hovered entity, grid visuals, gizmo state (`GizmoOp` + `GizmoMode` + per-op snap step sizes engaged by Ctrl-hold during drag), tool state (`ToolMode` Transform/TilePaint + `SelectedTileIndex`), and `FallbackEnvironment` (engine-default HDR's equirect / skybox / irradiance / prefilter cubemaps, loaded once at boot by `EditorLayer::LoadFallbackEnvironment` from `resources/environments/default.hdr` — layered onto envless scenes in edit mode only).
- `src/panels/`: Self-contained UI panels, each with an `OnImGuiRender()` method.
  - `scene_hierarchy_panel`: Entity tree view and component inspector.
  - `content_browser_panel`: Asset file browser.
  - `viewport_panel`: Owns the multi-stage framebuffer chain (HDR RGBA16F scene → LDR intermediate → FXAA-final), editor grid, mouse picking, ImGuizmo wiring (translate/rotate/scale with `SetAlternativeWindow` hooked to the viewport, drag-batched into a single `TransformEditCommand` for undo; Ctrl-hold during drag engages snapping), tile paint overlay (cell grid + hover highlight + LMB-held paint when `ToolMode == TilePaint`), and per-camera fixed-aspect game-view letterbox math (`ComputeGameViewRect`, `mGameViewOffset` — Play mode shrinks the post chain to the camera's aspect and pads black bars). Skybox drawing is delegated to `Renderer3D::DrawSkybox` so editor + runtime share one impl.
  - `toolbar_panel`: Floating dynamic-island toolbar (gizmo tool selection, play/stop, settings popup with editor view-state — camera, grid — and a DEBUG VIZ section for Skybox Source + Mesh Debug Viz).
  - `scene_properties_panel`: Unreal-style "World Settings" window. Hosts scene-level properties that get serialized into the `.loom` (currently just the Skybox HDR path; designed to grow — ambient color, fog, default gravity, etc.).
- `src/editor/`: Business logic managers (no ImGui rendering except for their own modals).
  - `scene_manager`: Scene I/O (New/Open/Save/SaveAs), play/stop transitions, "Save Changes?" modal.
  - `project_manager`: Project I/O (New/Open/SaveAs), "New Project Wizard" modal.
- `src/scripts/`: Native C++ scripts for sandbox/testing purposes (e.g., `player_controller`).

## `sandbox/` — Example Project

An example project built with Loom Engine that demonstrates how to use the engine's API and features correctly. It is the reference for how a user-side game project should be structured.

## `resources/` — Engine Assets

Shaders (`.glsl`/`.vert`/`.frag`), fonts, and icons used by the engine and editor. Copied to the build output directory automatically.

---

# 4. Architecture Patterns & Code Conventions

## Design

- **Layer-based Architecture:** The application (`Loom::Application`) manages a `LayerStack`. The editor runs as a single `EditorLayer`. Game logic runs in layers pushed onto the stack.
- **ECS-Driven:** Game objects are `Loom::Entity` handles inside a `Loom::Scene`. Components are plain data structs with no logic (defined in `components.h`). All behavior is in systems.
- **Abstract Renderer API:** `RenderCommand` and `Renderer2D` call through a virtual `RendererAPI`. Platform implementations (OpenGL) live in `platform/opengl/` — never add platform-specific code to `engine/renderer/`.

## Naming & Files

- **File Naming:** `snake_case.h` / `snake_case.cpp` (e.g., `scene_hierarchy_panel.cpp`)
- **Class & Struct Naming:** `PascalCase` (e.g., `EditorLayer`, `TransformComponent`)
- **Member Variables:** `mPascalCase` prefix (e.g., `mActiveScene`, `mViewportSize`)
- **Local Variables & Parameters:** `snake_case` (e.g., `grid_transform`, `out_path`)

## Includes

- Engine headers: angle brackets with `loom/` prefix — `#include <loom/scene/scene.h>`
- Vendor headers: angle brackets — `#include <imgui.h>`, `#include <glm/glm.hpp>`
- Local/project headers: quoted relative paths — `#include "editor_context.h"`

## Macros

- `LOOM_BIND_EVENT_FN(fn)` — binds a member function for event dispatching
- `LOOM_CORE_TRACE/INFO/WARN/ERROR(...)` — spdlog-backed engine logging
- `LOOM_ASSERT(cond, msg)` — debug assertion

---

# 5. Dependency Management (Vendor Libraries)

- **Git Submodules Only:** We exclusively use Git Submodules for third-party libraries in the `vendor/` directory. You are STRICTLY FORBIDDEN from using CMake `FetchContent` or `ExternalProject`.
- **Do Not Execute Submodule Commands:** If a new library is needed for a task, DO NOT attempt to run `git submodule add` yourself.
- **User Execution:** Instead, provide the exact `git submodule add <repository_url> vendor/<library_name>` command, explain why the library is needed, and WAIT for confirmation that it has been executed.
- **CMake Integration:** Only after confirmation that the submodule has been successfully added, proceed to update `cmake/vendors.cmake` using `add_subdirectory()` (or a manual `add_library` block if the library has no CMakeLists.txt), then update the vendor table in this file.

---

# 6. Git Workflow & Commit Guidelines

- **Validation-gated Commit Suggestions:** Work proceeds on a strict **Work → Validation → Commit → Repeat** cycle. After implementing a slice, provide the build command and a concrete validation recipe, then *stop*. Do NOT propose the commit message yet. Only once the user explicitly confirms the slice works ("works fine", "validated", etc.) — *then* immediately provide the exact `git commit` command with the appropriate Conventional Commit message. After sending the commit message, wait for the user to confirm commit + "go ahead" before starting the next slice.
- **Conventional Commits format:**
  - `feat:` — new feature
  - `fix:` — bug fix
  - `refactor:` — code restructuring with no behavior change
  - `style:` — formatting, whitespace
  - `chore:` — build, config, dependency updates
  - `docs:` — documentation only
- **Message structure:** Concise subject line (imperative mood, ≤72 chars). For complex changes, add a short body explaining *what* changed and *why*.

---

# 7. Strict Context & File Access Limits

- **VENDOR IS A BLACKBOX:** You are STRICTLY FORBIDDEN from reading, searching, or analyzing any files inside the `vendor/` directory.
- Do not use commands like `cat`, `grep`, `rg`, or `ls` on `vendor/`.
- Assume all third-party libraries in `vendor/` work correctly according to their standard public APIs. Do not waste context window reading their source code.

---

# 8. Documentation Maintenance

- **CLAUDE.md auto-update:** Continuously monitor architectural changes, new vendor libraries, and coding conventions. Whenever a significant change occurs (new scripting language, major core system, changed architecture pattern), proactively update this file. Do not wait to be asked.
- **PROJECT_LOG.md auto-update:** Always update `PROJECT_LOG.md` when completing a step, adding a new feature, or making significant architectural decisions. New entries to the Engineering Log go at the top; roadmap items get `[x]` markers as they ship. Do not wait to be asked.
- **README maintenance:** Upon completing a major roadmap milestone (Audio, Physics, WeaverRuntime, etc.), review and propose updates to `README.md`. Keep "Features", "Current State", and "Dependencies" aligned with the codebase. Do NOT update `README.md` for minor bug fixes, UI tweaks, or micro-steps.

---

# 9. Validation & Testing

- **Validation & Testing:** Whenever you complete a feature, system, or a logical chunk of work, you MUST proactively provide a concrete way for me to test and validate those changes. This could be a short code snippet to insert into the `sandbox/` application, a specific UI action to perform in `weaver/`, or a simple debug log statement using `spdlog`. Do not leave me guessing how to verify the code.
- **Runtime log file:** The engine tees every `LOOM_/LOOM_CORE_` line to `logs/loom.log` (CWD-relative — for a normal CMake build that's `build/<config>/bin/logs/loom.log`, next to the Weaver / WeaverRuntime exe). File is truncated each `Log::Init` so it contains exactly the latest session, no scrollback. Grep / `Read` this file when diagnosing what just happened instead of relying on console paste.

---

# 10. Communication Style

- **No code previews when proposing edits:** Do NOT paste multi-line code blocks showing what you are *about to* write. Describe the change at a high level (file, intent, key symbols/dependencies) and then execute it — the `Edit`/`Write` tool call already surfaces the diff. Previewing the precise contents before writing is redundant and verbose.

---

# 11. Division of Labor

- **Claude (you):** Write all *functional* code, including the editor application. This explicitly includes:
  - Engine C++ (renderer, ECS, scripting, asset, project, math, platform code).
  - **All functional ImGui / Editor code** — panel structure, widget logic, event handling, input routing, `ImGui::GetWindowDrawList()` rendering math, interaction state machines, picking / hit-testing, drag logic, modal flows.
  - All editor business logic (managers, commands, history, serialization wiring).
  - **Do NOT wait for the human to "wire up the UI side"** — if a feature requires ImGui code to be functional, you write it.
- **Human (the user):** Owns UX polish only — UX design decisions, layout tweaking, styling, colors, font sizes, spacing, iconography. The human refines the look and feel of UI you have already made functional; they do not implement UI logic.
- **When in doubt:** if code is needed to make a feature *work*, it belongs to Claude. Human edits sit on top of working code, not in place of it.

---

# Your Mission

Sections 4 (Architecture & Conventions) and 5–8 cover the rules. Two stand-alone reminders:

- **Build system:** register every new `.cpp` file in the appropriate `CMakeLists.txt` so the file actually compiles.
- **Never build yourself:** do NOT run `cmake --build`, `ninja`, `make`, or any compiler invocation. After writing code, provide the exact build command for me to run and wait for me to report errors.
