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

# 2. Tech Stack & Vendor Libraries
Third-party libraries are located in the `vendor/` directory. Always use these instead of rolling your own:

| Purpose | Library | Vendor Dir |
|---|---|---|
| ECS (Entity Component System) | EnTT | `entt` |
| Windowing & Input | GLFW | `glfw` |
| OpenGL Loader | GLAD | `glad` |
| UI / Editor | Dear ImGui + ImGuizmo | `imgui`, `imguizmo` |
| Math | GLM | `glm` |
| Logging | spdlog | `spdlog` |
| Image Loading | stb_image | `stb` |
| Physics (2D) | Box2D | `box2d` |
| Serialization | YAML-CPP | `yaml-cpp` |
| File Dialogs | nativefiledialog-extended | `nfd` |
| Scripting VM | Lua 5.4 | `lua` |
| Lua C++ Bindings | sol2 v3.5.0 | `sol2` |

# 3. Directory Structure Architecture

## `engine/` — Core Engine Library
The engine compiles to a static/dynamic library. Internal headers are exposed under the `<loom/...>` include prefix (e.g., `#include <loom/scene/scene.h>`).

- `core/`: Application loop, LayerStack, Events system, Input, Window abstraction, Timestep, UUID, and core macros (`LOOM_BIND_EVENT_FN`, `LOOM_CORE_*` log macros).
- `renderer/`: Abstract Renderer API, Shaders, Textures, Buffers, Framebuffers, VertexArray, Cameras (`OrthographicCamera`, `EditorCamera`), Renderer2D.
- `scene/`: ECS implementation. Contains `scene.cpp`, `entity.cpp`, `components.h` (all component structs including `LuaScriptComponent`), `scene_serializer`, and `script_registry`.
- `asset/`: `AssetManager` — centralized loader/cache for shaders and textures.
- `project/`: `Project` and `ProjectSerializer` — manage project config (name, asset directory, start scene).
- `math/`: Engine math utilities (e.g., `Math::DecomposeTransform`).
- `platform/`: Platform-specific implementations (e.g., `platform/opengl/` for OpenGL buffer/shader/texture implementations, `platform/windows/` for input and window).
- `scripting/`: Scripting subsystem.
  - `scripting_engine.h/.cpp` (public): Singleton facade. `Init()` creates the Lua backend; `Shutdown()` tears it down. `OnRuntimeStart/Update/Stop` are forwarded by `Scene`. Initialized automatically by `Application`.
  - `backends/scripting_backend.h` (private): `IScriptingBackend` pure-virtual interface (`OnRuntimeStart`, `OnRuntimeUpdate`, `OnRuntimeStop`, `OnCollisionBegin`, `OnCollisionEnd`, `OnFileChanged`).
  - `backends/lua/lua_scripting_backend.h/.cpp` (private): Concrete Lua 5.4.4 + sol2 v3.5.0 backend. Manages one `sol::state`, per-entity `sol::environment` instances, and hot-reload via `OnFileChanged`. Binds `Vec2`, `Vec3`, `Entity` (transform/tag/audio/physics accessors), `Input`, `Key`, `Mouse`, `Log` to Lua.

### Lua Script API (for `LuaScriptComponent` scripts)
Each script runs in an isolated `sol::environment`. The global `entity` is a handle to the owning entity.
```lua
function OnCreate()  end        -- called once at runtime start
function OnUpdate(ts) end       -- called every frame; ts = delta time (seconds)
function OnDestroy() end        -- called at runtime stop
function OnCollisionBegin(other) end  -- called when this entity's collider first touches another
function OnCollisionEnd(other) end    -- called when this entity's collider stops touching another
function OnSensorBegin(other) end     -- called when another entity enters this entity's sensor collider
function OnSensorEnd(other) end       -- called when another entity exits this entity's sensor collider

-- Available globals: entity, Input, Key, Mouse, Log, Vec2, Vec3, Scene
-- entity:GetTranslation() / SetTranslation(vec3)
-- entity:GetRotation()    / SetRotation(vec3)
-- entity:GetScale()       / SetScale(vec3)
-- entity:GetTag() -> string
-- entity:FindByTag(tag) -> entity
-- entity:Spawn() -> entity          (creates a new blank entity in the same scene)
-- entity:Destroy()                  (destroys this entity)
-- entity:Instantiate(path) -> entity (instantiates a .lprefab file)
-- Input.IsKeyPressed(Key.W), Input.GetMouseX(), etc.
-- Physics.Raycast(origin_vec3, dir_vec3, distance) -> { hit, point, normal, entity }
-- Physics.OverlapCircle(center_vec2, radius) -> array of entities
-- Physics.OverlapBox(center_vec2, half_extents_vec2) -> array of entities
-- Scene transitions (queued end-of-frame; path is relative to asset directory):
--   Scene.Load("scenes/level2.loom")  -- load a different scene file
--   Scene.Reload()                    -- restart the current scene from its last saved state
-- Audio (requires AudioSourceComponent):
--   entity:PlayAudio()              entity:StopAudio()
--   entity:IsAudioPlaying() -> bool
--   entity:SetVolume(v)             entity:SetPitch(p)
-- Physics body (requires Rigidbody2DComponent):
--   entity:SetLinearVelocity(vec2)  entity:GetLinearVelocity() -> vec2
--   entity:ApplyForce(vec2)         entity:ApplyImpulse(vec2)
-- Collision callbacks (no component requirement beyond the collider itself):
--   OnCollisionBegin(other_entity)  / OnCollisionEnd(other_entity)  -- both entities notified
```

## `weaver/` — Editor Application
Built on top of the engine. All editor code is in the `Weaver::` namespace.

- `src/editor_layer.h/.cpp`: Thin orchestrator layer. Owns the `EditorContext`, all panels, and all manager objects. Handles the ImGui dockspace, menu bar, and input routing.
- `src/editor_context.h`: Shared mutable state struct (`EditorContext`) and `GridSettings`. Passed by reference to all panels and managers to avoid tight coupling.
- `src/panels/`: Self-contained UI panels, each with an `OnImGuiRender()` method.
  - `scene_hierarchy_panel`: Entity tree view and component inspector.
  - `content_browser_panel`: Asset file browser.
  - `viewport_panel`: Owns the framebuffer, skybox, editor grid, mouse picking, and gizmo rendering.
  - `toolbar_panel`: Floating dynamic-island toolbar (gizmo tool selection, play/stop, settings popup).
- `src/editor/`: Business logic managers (no ImGui rendering except for their own modals).
  - `scene_manager`: Scene I/O (New/Open/Save/SaveAs), play/stop transitions, "Save Changes?" modal.
  - `project_manager`: Project I/O (New/Open/SaveAs), "New Project Wizard" modal.
- `src/scripts/`: Native C++ scripts for sandbox/testing purposes (e.g., `player_controller`).

## `sandbox/` — Example Project
An example project built with Loom Engine that demonstrates how to use the engine's API and features correctly. It is the reference for how a user-side game project should be structured.

## `resources/` — Engine Assets
Shaders (`.glsl`/`.vert`/`.frag`), fonts, and icons used by the engine and editor. Copied to the build output directory automatically.

# 4. Architecture Patterns & Code Conventions

### Design
- **Layer-based Architecture:** The application (`Loom::Application`) manages a `LayerStack`. The editor runs as a single `EditorLayer`. Game logic runs in layers pushed onto the stack.
- **ECS-Driven:** Game objects are `Loom::Entity` handles inside a `Loom::Scene`. Components are plain data structs with no logic (defined in `components.h`). All behavior is in systems.
- **Abstract Renderer API:** `RenderCommand` and `Renderer2D` call through a virtual `RendererAPI`. Platform implementations (OpenGL) live in `platform/opengl/` — never add platform-specific code to `engine/renderer/`.

### Naming & Files
- **File Naming:** `snake_case.h` / `snake_case.cpp` (e.g., `scene_hierarchy_panel.cpp`)
- **Class & Struct Naming:** `PascalCase` (e.g., `EditorLayer`, `TransformComponent`)
- **Member Variables:** `mPascalCase` prefix (e.g., `mActiveScene`, `mViewportSize`)
- **Local Variables & Parameters:** `snake_case` (e.g., `grid_transform`, `out_path`)

### Includes
- Engine headers: angle brackets with `loom/` prefix — `#include <loom/scene/scene.h>`
- Vendor headers: angle brackets — `#include <imgui.h>`, `#include <glm/glm.hpp>`
- Local/project headers: quoted relative paths — `#include "editor_context.h"`

### Macros
- `LOOM_BIND_EVENT_FN(fn)` — binds a member function for event dispatching
- `LOOM_CORE_TRACE/INFO/WARN/ERROR(...)` — spdlog-backed engine logging
- `LOOM_ASSERT(cond, msg)` — debug assertion

# 5. Dependency Management (Vendor Libraries)
- **Git Submodules Only:** We exclusively use Git Submodules for third-party libraries in the `vendor/` directory. You are STRICTLY FORBIDDEN from using CMake `FetchContent` or `ExternalProject`.
- **Do Not Execute Submodule Commands:** If a new library is needed for a task, DO NOT attempt to run `git submodule add` yourself.
- **User Execution:** Instead, provide the exact `git submodule add <repository_url> vendor/<library_name>` command, explain why the library is needed, and WAIT for confirmation that it has been executed.
- **CMake Integration:** Only after confirmation that the submodule has been successfully added, proceed to update `cmake/vendors.cmake` using `add_subdirectory()` (or a manual `add_library` block if the library has no CMakeLists.txt), then update the vendor table in this file.

# 6. Git Workflow & Commit Guidelines
- **Autonomous Commit Suggestions:** You must independently decide when a logical chunk of work (refactor, feature, bug fix) is complete. Once you determine it is time to commit, DO NOT ask for permission. Immediately provide the exact `git commit` command with the appropriate Conventional Commit message for me to execute, or execute it if permitted.
- **Conventional Commits format:**
  - `feat:` — new feature
  - `fix:` — bug fix
  - `refactor:` — code restructuring with no behavior change
  - `style:` — formatting, whitespace
  - `chore:` — build, config, dependency updates
  - `docs:` — documentation only
- **Message structure:** Concise subject line (imperative mood, ≤72 chars). For complex changes, add a short body explaining *what* changed and *why*.

# 7. Strict Context & File Access Limits
- **VENDOR IS A BLACKBOX:** You are STRICTLY FORBIDDEN from reading, searching, or analyzing any files inside the `vendor/` directory.
- Do not use commands like `cat`, `grep`, `rg`, or `ls` on `vendor/`.
- Assume all third-party libraries in `vendor/` work correctly according to their standard public APIs. Do not waste context window reading their source code.

# 8. CLAUDE.md Maintenance
- **Auto-Update CLAUDE.md:** Continuously monitor the project's architectural changes, new vendor libraries, and coding conventions. Whenever a significant change occurs (e.g., integrating a new scripting language, adding a major core system, or changing architecture patterns), proactively update this `CLAUDE.md` file to reflect the current and accurate state of the Loom Engine. Do not wait to be asked.

# 9. Validation & Testing
- **Validation & Testing:** Whenever you complete a feature, system, or a logical chunk of work, you MUST proactively provide a concrete way for me to test and validate those changes. This could be a short code snippet to insert into the `sandbox/` application, a specific UI action to perform in `weaver/`, or a simple debug log statement using `spdlog`. Do not leave me guessing how to verify the code.

# 10. Development Roadmap

Keep this section current. Mark completed items with `[x]`, update priorities as the project evolves.

---

## Phase 1 — 2D Feature Complete

*Goal: everything a 2D game needs before shipping via WeaverRuntime.*

- [x] **Audio extensions + Lua component bindings**
  - `AudioSourceComponent`: add `Pitch` (float, `ma_sound_set_pitch`) and `Pan` (float -1..1, `ma_sound_set_pan`); inspector + YAML
  - Lua audio API on `entity`: `PlayAudio()`, `StopAudio()`, `IsAudioPlaying()`, `SetVolume(v)`, `SetPitch(p)`
  - Lua physics API on `entity`: `SetLinearVelocity(vec2)`, `GetLinearVelocity()`, `ApplyForce(vec2)`, `ApplyImpulse(vec2)` via `Rigidbody2DComponent`

- [x] **Physics scripting — collision callbacks** *(sub-item A)*
  - After physics step in `Scene::OnUpdateRuntime`, call `b2World_GetContactEvents()` and dispatch `OnCollisionBegin(other_entity)` / `OnCollisionEnd(other_entity)` to Lua scripts on both involved entities
  - Enable `b2ShapeDef.enableContactEvents = true` on all shapes at creation (no new component field)
  - Guards: entity must have `LuaScriptComponent`; skip if script env missing

- [x] **Physics scripting — sensor / trigger colliders** *(sub-item B)*
  - Add `IsSensor` bool to `BoxCollider2DComponent` and `CircleCollider2DComponent`; inspector checkbox + YAML (default `false`)
  - Sensors set `b2ShapeDef.isSensor = true` + `b2ShapeDef.enableSensorEvents = true`
  - After physics step, call `b2World_GetSensorEvents()` and dispatch `OnSensorBegin(other_entity)` / `OnSensorEnd(other_entity)` to Lua scripts on both entities

- [x] **Physics scripting — spatial overlap queries** *(sub-item C)*
  - `Physics.OverlapCircle(center_vec2, radius)` → Lua array of entities
  - `Physics.OverlapBox(center_vec2, half_extents_vec2)` → Lua array of entities
  - Uses Box2D world AABB/shape query API; no new components required

- [x] **Editor quit / unsaved-changes confirmation**
  - `Application::Close()` public method added; `Application::OnEvent` reordered so layers handle events before built-in handlers (enables interception)
  - `SceneManager::RequestQuit()`: shows "Save Changes?" modal if dirty, calls `Close()` directly if clean
  - `EditorLayer` intercepts `WindowCloseEvent` and routes to `RequestQuit()`; `Ctrl+Q` shortcut + `File > Exit` menu item added
  - Fixed pre-existing bug: `SetSceneModifiedCallback` was setting `SceneDirty = false` instead of `true`

- [x] **Scene transitions** *(prerequisite for WeaverRuntime — games need level loading)*
  - Engine-side `SceneLoader` singleton (no editor dependency): queues a scene path to load at end of frame
  - Lua API: `Scene.Load("path/to/scene.loom")`, `Scene.Reload()`
  - `EditorLayer` polls `SceneLoader` after each `OnUpdateRuntime` and delegates to `SceneManager::OnRuntimeSceneTransition`; `RuntimeLayer` will do the same

- [x] **Script Property Exposure System** *(Script Field Binding)*

  *Goal: allow scripts to expose typed variables to the Properties Panel so the Game Developer can override defaults per-entity in Weaver without editing code. The data model is language-agnostic so future backends plug in cleanly.*

  - [x] **`feat(scripting):` Language-agnostic `ScriptField` data model**
    - `ScriptFieldType` enum: `Float`, `Int`, `Bool`, `Vec2`, `Vec3`, `String` — lives in `engine/include/loom/scripting/script_field.h`
    - `ScriptField` struct: `Name` (string), `Type`, `Value` (`std::variant<float, int, bool, glm::vec2, glm::vec3, std::string>`)
    - `LuaScriptComponent` gains `std::unordered_map<std::string, ScriptField> Fields` for editor-set overrides
    - `IScriptingBackend` extended with three new pure-virtuals: `GetScriptFields`, `ApplyFields`, `TryGetFieldValue`
    - `SceneSerializer` reads/writes the `Fields:` block under `LuaScriptComponent` in YAML; prefab deserializer also updated

  - [x] **`feat(scripting):` Lua field discovery & value injection**
    - `Properties = { Speed = 5.0, Health = 100 }` top-level table is the schema convention
    - `LuaScriptingBackend::GetScriptFields()` runs a sandboxed `sol::state`, reads the `Properties` table, infers type; results are cached by absolute path and invalidated on `OnFileChanged`
    - `LoadEntityScript` injects editor overrides from `lsc.Fields` into the `sol::environment` before `OnCreate()` is called
    - `TryGetFieldValue` reads live globals from a running environment for real-time inspector display

  - [x] **`feat(editor):` Script fields ImGui drawer in Properties Panel**
    - Inspector block below the script path row renders one widget per schema field: `DragFloat`, `DragInt`, `Checkbox`, `InputText`, `DragFloat2`, `DragFloat3`
    - Edits write to `LuaScriptComponent::Fields` directly and mark scene dirty
    - In Play mode fields are `BeginDisabled`/read-only and show live runtime values via `TryGetFieldValue`; in Edit mode fully editable
    - `SceneManager::OnScenePlay/Stop` calls `HierarchyPanel::SetPlayMode` to switch modes

---

## Phase 1.5 — Project System Hardening *(prerequisite for WeaverRuntime)*

*Goal: make the `.loomproj` file and `Project`/`ProjectManager` robust enough to be the single source of truth for WeaverRuntime.*

- [x] **`chore(project):` Schema hardening**
  - `Version: 1` added to `ProjectConfig` and written by `ProjectSerializer::Serialize`
  - `Deserialize` warns on missing version, warns on future version, errors on missing/empty `AssetDirectory` or non-existent path on disk, warns on missing `StartScene`
  - `ProjectManager::OpenProject` shows a "Project Load Error" modal instead of silently dropping the failure

- [x] **`feat(project):` Runtime window config**
  - `WindowTitle`, `WindowWidth`, `WindowHeight` added to `ProjectConfig` + `ProjectSerializer` (serialized/deserialized)
  - "Project Settings..." menu item added to File menu (disabled when no active project)
  - `ProjectManager::OpenSettings()` modal: Project Name, Start Scene (InputText + NFD browse), Window Title, Width, Height, Apply/Cancel
  - *(Merges and closes the "Project config expansion" item that was previously in Phase 2)*

- [x] **`chore(project):` ProjectManager null-safety & state**
  - Guard every `Project::GetActive()` dereference; a missing active project must never silently corrupt state
  - Add a "recently opened projects" list (persisted to `editor_prefs.yaml` in the user config directory)

---

## Phase 2 — WeaverRuntime (Standalone Export)

*Goal: a project saved from Weaver runs as a standalone executable.*

- [x] **`weaver_runtime/` CMake target**
  - `weaver_runtime/` directory alongside `weaver/`; own `main.cpp`; links only `Loom` (no `nfd`, no editor deps)
  - `Application(const WindowProps& props)` constructor added to engine; default ctor delegates to it
  - `main.cpp` early-loads the project config to extract `WindowTitle/Width/Height` before `Application` is constructed

- [x] **`RuntimeLayer`**
  - `OnAttach`: reads `Project::GetActive()` config → `LoadScene(StartScene)` → `Scene::OnRuntimeStart()`
  - `OnUpdate(ts)`: `RenderCommand::Clear()` → `Scene::OnUpdateRuntime(ts)` (handles scripts + physics + rendering via primary `SceneCamera`)
  - `OnDetach`: `Scene::OnRuntimeStop()`
  - Polls `SceneLoader` queue after each update; handles both `IsReload()` and path-based transitions

- [x] **Packaging validation**
  - Verify a Weaver project loads and runs correctly in `WeaverRuntime`
  - Confirm all asset paths resolve correctly relative to the executable
  - Fixed: `GetEngineAssetDirectory()` returned a relative path; anchored cwd to exe dir via `GetModuleFileNameW` in `WeaverRuntime/main.cpp` before engine init; project path is made absolute before the cwd change

---

## Phase 3 — Editor & Tools Polish

*Goal: close daily workflow gaps before committing to 3D.*

- [x] **Undo / Redo system + Dirty Checking (Command Pattern)**

  - [x] **`feat(editor):` `IEditorCommand` interface + `EditorHistory` manager**
    - `IEditorCommand`: `Execute()`, `Undo()`, `GetDescription() → string`
    - `EditorHistory`: fixed 50-step stack living in `EditorContext`; `Ctrl+Z` / `Ctrl+Y` keybindings wired in `EditorLayer`

  - [x] **`feat(editor):` Core command implementations**
    - `EntityCreateCommand`, `EntityDeleteCommand`
    - `AddComponentCommand`, `RemoveComponentCommand`
    - `TransformEditCommand` (batches gizmo drag deltas into one undoable step)
    - `PropertyEditCommand<T>` (generic template for inspector field edits)

  - [x] **`refactor(editor):` History-driven dirty checking**
    - Replace scattered `SceneDirty = true` calls with `EditorHistory::MarkSavePoint()` on save; dirty = `history_depth != save_point_depth`
    - Title-bar `*` and "Save Changes?" modal remain behaviorally identical, now driven by history stack depth rather than an ad-hoc boolean

- [x] **Text / HUD rendering** *(suggestion)*
  - Add **stb_truetype** (single-header, already in `vendor/stb` family)
  - `FontAsset`: TTF → glyph atlas texture via `stb_truetype`
  - `TextComponent`: font path, text string, size, color
  - `Renderer2D::DrawText(...)` — batched quads from glyph atlas
  - Inspector UI + YAML serialization

- [x] **Tilemap component** *(scope-reduced — no .tmx import)*
  - `TilemapComponent`: grid dimensions, tile size, spritesheet `Texture2D`, `std::vector<int>` tile index data
  - `Renderer2D::DrawTilemap(...)` — single batched draw call per layer
  - Inspector: tile grid editor (click to paint index)
  - YAML serialization

- [ ] **Particle system** *(suggestion, lower priority)*
  - `ParticleComponent`: emitter shape, spawn rate, lifetime, velocity range, size/color over lifetime
  - CPU-simulated, rendered via batched `Renderer2D` quads
  - Inspector UI + YAML

---

## Phase 4 — 3D Foundation

*Correct dependency order: assets first, renderer second, lighting third.*

- [ ] **Mesh loading**
  - Add **cgltf** submodule (`vendor/cgltf`, single C file)
  - `MeshAsset`: VAO/VBO storing position, normal, UV, index data
  - `AssetManager::LoadMesh(path)` — loads and caches GLTF/GLB

- [ ] **Mesh & material components**
  - `MeshComponent`: path to a GLTF/GLB asset
  - `MaterialComponent`: albedo color/texture, roughness, metallic
  - `MeshRendererComponent`: references mesh + material

- [ ] **Renderer3D**
  - `Renderer3D::Submit(mesh, material, transform)` draw call
  - Independent pipeline from `Renderer2D` (own shaders, own VAO setup)
  - Camera integration: reuse `EditorCamera` perspective projection

- [ ] **Basic lighting**
  - `DirectionalLightComponent`, `PointLightComponent`
  - Phong shading pass in `Renderer3D`

- [ ] **3D physics**
  - Add **Jolt Physics** submodule (`vendor/jolt`)
  - `PhysicsEngine3D` singleton alongside Box2D
  - `Rigidbody3DComponent`, `BoxCollider3DComponent`, `SphereCollider3DComponent`
  - `Scene::OnPhysicsStart3D` / `OnPhysicsStop3D`

---

## Phase 5 — Advanced Rendering

- [ ] **PBR shading** — Replace Phong with metallic-roughness PBR; IBL environment maps
- [ ] **Shadow mapping** — Directional shadow maps; cascaded shadows for large scenes

---

## Phase 6 — Graphics API Expansion

*Start only after Phase 4 is complete and stable. The abstract `RendererAPI` / `RenderCommand` layer is already designed for multi-backend support.*

- [ ] **Vulkan backend** — `platform/vulkan/`; requires Vulkan SDK + **VMA** + **vk-bootstrap** submodules; GLSL → SPIR-V via `shaderc`
- [ ] **DirectX 12 backend** — `platform/directx12/`; Windows SDK only; HLSL → DXIL via `dxc`

---

## Candidate Vendor Libraries (not yet added)

| Library      | Submodule path        | Purpose                                   |
|--------------|-----------------------|-------------------------------------------|
| cgltf        | `vendor/cgltf`        | GLTF/GLB mesh loading (single-header C)   |
| Jolt Physics | `vendor/jolt`         | 3D rigid-body physics (C++17, MIT)        |
| VMA          | `vendor/vma`          | Vulkan Memory Allocator                   |
| vk-bootstrap | `vendor/vk-bootstrap` | Vulkan instance/device init boilerplate   |

---

## Documentation Protocol
- **README Maintenance:** Upon completing a major roadmap milestone (e.g., implementing a new core system like Audio, Physics, or WeaverRuntime), you must automatically review and propose updates to `README.md`.
- Ensure the "Features", "Current State", and "Dependencies" sections are always aligned with our actual codebase.
- Do NOT update `README.md` for minor bug fixes, UI tweaks, or micro-steps.

## Completed

- **Tilemap component** — `TilemapComponent` struct (`SpritesheetPath`, `Spritesheet` runtime handle, `Columns`/`Rows` grid dims, `TileWidth`/`TileHeight` world-space tile size, `SheetColumns`/`SheetRows` spritesheet layout, `std::vector<int> Tiles` flat row-major array, -1 = empty); `Renderer2D::DrawTilemap(...)` iterates the tile array and submits per-tile quads with computed UV rects into the existing quad batch (all tiles share the same texture slot); render loop wired in both `Scene::OnUpdateEditor` and `Scene::OnUpdateRuntime` via `DrawTilemapEntity` helper; YAML serialization round-trips all fields plus the tile array as a compact flow sequence; inspector block in `SceneHierarchyPanel` exposes spritesheet drag-slot, grid dims, tile size, sheet layout, and a scrollable click-to-paint tile grid with paint-index selector (arrow buttons) and Clear All; no new CMake source files required.
- **Text / HUD rendering** — `stb_truetype` (single-header, already in `vendor/stb` family) used to rasterize TTF glyphs into an atlas texture; `FontAsset` loads a TTF file and produces a `Texture2D` glyph atlas with per-glyph UV/advance metrics; `TextComponent` stores font path, text string, size, and color; `Renderer2D::DrawText(...)` submits batched quads from the glyph atlas into the existing sprite batch pipeline; inspector UI (font drag-slot, text field, size/color widgets) + YAML serialization round-trip.
- **ProjectManager null-safety & state** — Every `Project::GetActive()` dereference guarded against null (missing active project can no longer silently corrupt state); recently opened projects list persisted to `editor_prefs.yaml` in the user config directory; list surfaced in the editor for quick re-open.
- **Undo / Redo system (Command Pattern)** — `IEditorCommand` / `EditorHistory` (50-step deque with save-point tracking) live in `weaver/src/editor/`; six concrete commands: `EntityCreateCommand` (re-uses UUID on redo), `EntityDeleteCommand` (snapshots to YAML for UUID-preserving restore), `AddComponentCommand<T>`, `RemoveComponentCommand<T>` (captures data before removal), `TransformEditCommand` (committed on gizmo mouse-up), `PropertyEditCommand<T>` (generic before/after with setter lambda); `Ctrl+Z` / `Ctrl+Shift+Z` / `Ctrl+Y` wired in `EditorLayer`; `SceneHierarchyPanel` routes all create/delete/add-component/remove-component/transform/property edits through `mCommandCallback`; `EditorContext::IsDirty()` ORs `SceneDirty` (non-history edits) with `History.IsDirty()` (history depth vs. save-point depth); save calls `MarkSavePoint()`, new/open call `Clear()`; title-bar `*` and "Save Changes?" modal driven by `IsDirty()`; undo correctly clears the dirty indicator.
- **WeaverRuntime packaging validation** — Fixed engine resource path resolution: `WeaverRuntime/main.cpp` anchors cwd to the executable directory via `GetModuleFileNameW` before engine init (project path is made absolute first to preserve relative user-supplied args); all `resources/` assets now resolve correctly from any launch directory.
- **WeaverRuntime standalone executable** — `weaver_runtime/` CMake target added alongside `weaver/`; links only `Loom` (no `nfd`/editor deps); `Application(const WindowProps&)` constructor added to engine (default ctor delegates); `main.cpp` early-deserializes the project config to configure window title/size; `RuntimeLayer` loads start scene via `SceneSerializer`, calls `Scene::OnRuntimeStart/UpdateRuntime/Stop`, handles `WindowResizeEvent` to keep `OnViewportResize` in sync, and polls `SceneLoader` for Lua-driven scene transitions each frame.
- **Runtime window config + Project Settings** — `WindowTitle`, `WindowWidth`, `WindowHeight` added to `ProjectConfig` and serialized/deserialized by `ProjectSerializer`; `ProjectManager::OpenSettings()` populates temp buffers from the active config; "Project Settings..." modal exposes Name, Start Scene (with NFD browse), Window Title, Width, Height; "Project Settings..." menu item added to File menu (disabled when no project is open); settings round-trip through the `.loomproj` file.
- **Project schema hardening** — `Version: 1` added to `ProjectConfig`; `ProjectSerializer::Deserialize` validates version (warn if missing/future), errors on missing/non-existent `AssetDirectory`, warns on missing `StartScene`; `ProjectManager` shows a "Project Load Error" modal on deserialization failure instead of silently proceeding.
- **Editor camera serialization** — `EditorCamera` gains `GetPitch()`, `GetYaw()`, and `SetState(position, pitch, yaw)`; `SceneSerializer::Serialize/Deserialize` accept an optional `EditorCamera*`; saves a top-level `EditorCamera:` block (Position, Pitch, Yaw) in the `.loom` file; `SceneManager::SaveScene`, `SaveSceneAs`, and `OpenSceneImpl` pass `&mContext.EditorCamera`; WeaverRuntime is unaffected (passes `nullptr` by default).
- **Script Property Exposure System** — `ScriptField` / `ScriptFieldType` added to `engine/include/loom/scripting/script_field.h`; `LuaScriptComponent` gains `Fields` map; `IScriptingBackend` extended with `GetScriptFields`, `ApplyFields`, `TryGetFieldValue`; `LuaScriptingBackend` implements field discovery via sandboxed `sol::state` with path-keyed cache, injects overrides before `OnCreate`, reads live values via `TryGetFieldValue`; `SceneSerializer` round-trips `Fields` block in YAML for both scenes and prefabs; `SceneHierarchyPanel` shows per-type widgets below the script path row, disabled in play mode showing live values; `SceneManager::OnScenePlay/Stop` toggles panel play mode.
- **Scene transitions** — `SceneLoader` singleton added to `engine/scene/`; Lua `Scene.Load(path)` / `Scene.Reload()` queue transitions end-of-frame; `EditorLayer::OnUpdate` polls `SceneLoader` after `OnUpdateRuntime` and calls `SceneManager::OnRuntimeSceneTransition`; `OnSceneStop` calls `Consume()` to discard stale queued transitions; `RuntimeLayer` polls the same `SceneLoader` queue each frame.
- **Editor quit / unsaved-changes confirmation** — `Application::Close()` added; event dispatch reordered so layers intercept first; `SceneManager::RequestQuit()` gates close behind the existing "Save Changes?" modal when dirty; `Ctrl+Q` shortcut and `File > Exit` menu item wired; `SceneDirty` callback bug fixed.
- **Physics scripting — spatial overlap queries** — `Physics.OverlapCircle(center, radius)` and `Physics.OverlapBox(center, half_extents)` added to the Lua `Physics` table; both return a 1-indexed Lua array of `Entity` handles; `Scene::OverlapCircle2D` / `Scene::OverlapBox2D` implemented via `b2World_OverlapCircle` / `b2World_OverlapPolygon` with per-shape callback; deduplication via `std::unordered_set` prevents duplicate entries when an entity holds multiple collider components.
- **Physics scripting — sensor / trigger colliders** — `IsSensor` bool added to `BoxCollider2DComponent` and `CircleCollider2DComponent`; sensor shapes set `b2ShapeDef.isSensor = true` + `enableSensorEvents = true` (mutually exclusive with `enableContactEvents`); `b2World_GetSensorEvents()` polled after physics step; `ScriptingEngine::OnSensorBegin/End(Entity, Entity)` dispatches to Lua `OnSensorBegin(other)` / `OnSensorEnd(other)` on both entities; `IScriptingBackend` extended with two new pure-virtual methods; inspector checkbox + YAML round-trip.
- **Physics scripting — collision callbacks** — `b2ShapeDef.enableContactEvents = true` on all shapes; `b2World_GetContactEvents()` polled after each physics step; `ScriptingEngine::OnCollisionBegin/End(Entity, Entity)` dispatches to Lua `OnCollisionBegin(other)` / `OnCollisionEnd(other)` callbacks on both involved entities; `IScriptingBackend` extended with two new pure-virtual methods.
- **Audio extensions + Lua bindings** — `AudioSourceComponent` extended with `Pitch` and `Pan` fields (miniaudio `ma_sound_set_pitch`/`ma_sound_set_pan`); `AudioEngine::SetVolume/SetPitch/IsPlaying` for runtime control; inspector sliders + YAML round-trip; Lua audio API (`PlayAudio`, `StopAudio`, `IsAudioPlaying`, `SetVolume`, `SetPitch`) and physics API (`SetLinearVelocity`, `GetLinearVelocity`, `ApplyForce`, `ApplyImpulse`) on `entity`; `Vec2` Lua type added.
- **Asset path normalization** — `ToRelativeAssetPath()` helper in serializer; all component paths (texture, Lua, audio) serialized relative to asset dir using `std::filesystem::relative()`.
- **Asset hot-reload** — `FileWatcher` embedded in `AssetManager`; `Reload()` on `Texture2D`/`Shader` updates GPU resources in-place; polled each frame from `EditorLayer::OnUpdate`.
- **Audio system** — `AudioEngine` singleton (miniaudio backend); `AudioSourceComponent` with path, volume, loop, autoplay; scene autoplay on `OnRuntimeStart`; inspector UI + YAML serialization.
- **Prefab system** — `.lprefab` YAML schema; `SceneSerializer` serialize/deserialize single entity; editor "Save as Prefab"; content browser drag to instantiate; `entity:Instantiate(path)` Lua binding.
- **Expanded Lua bindings** — `FindByTag`, `Spawn`, `Destroy`, `Instantiate`; `Physics.Raycast`; multi-arg `Log.*`.
- **Lua file watcher** — Background thread polls `last_write_time`; hot-reloads `.lua` scripts via `OnFileChanged`.
- **Circle Collider 2D** — `CircleCollider2DComponent` using Box2D `b2Circle`; wired into physics, serializer, and inspector.
- **Entity parent-child hierarchy** — `RelationshipComponent`; world transform via `Scene::GetWorldTransform`; drag-and-drop reparenting in hierarchy panel.
- **Content browser drag & drop** — Drag images onto texture slot; drag `.loom` onto viewport to open scene.
- **Viewport mouse-picking fix** — Guard `ImGuizmo::IsOver()` behind entity-selected + gizmo-active check to eliminate stale-frame false positives.
- **Sprite animation** — Frame-based `AnimationComponent` cycling UV regions at configurable FPS; serialized via YAML.
- **Spritesheet helper** — Auto-fills animation frames from sheet size, cell size, start row/col, and frame count.
- **TextureSpecification** — Per-texture `FilterMode`, `WrapMode`, `GenerateMips`; passed into `Texture2D::Create()`.

# Your Mission
When generating code, modifying files, or debugging:
1. **Respect separation of concerns:** Never put OpenGL-specific code in the abstract `engine/renderer/` layer — it belongs in `platform/opengl/`.
2. **Use existing libraries:** Use `spdlog` macros for logging, `glm` for all math, EnTT for entity queries.
3. **Editor UI:** Always use `ImGui` for new Weaver panels and windows.
4. **Build system:** Register every new `.cpp` file in the appropriate `CMakeLists.txt`.
5. **Patterns:** Follow existing naming conventions, include order, and member variable style before introducing new patterns.
6. **Never build the project yourself.** Do NOT run `cmake --build`, `ninja`, `make`, or any compiler invocation. After writing code, provide the user with the exact build command to run themselves and wait for them to report errors back.
