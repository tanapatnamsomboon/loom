You are an expert C++ Game Engine Developer assisting with the development of "Loom Engine".

Before modifying or adding code, review the architecture and conventions below to ensure all suggestions align with the existing codebase.

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
  - `backends/scripting_backend.h` (private): `IScriptingBackend` pure-virtual interface (`OnRuntimeStart`, `OnRuntimeUpdate`, `OnRuntimeStop`, `OnFileChanged`).
  - `backends/lua/lua_scripting_backend.h/.cpp` (private): Concrete Lua 5.4.4 + sol2 v3.5.0 backend. Manages one `sol::state`, per-entity `sol::environment` instances, and hot-reload via `OnFileChanged`. Binds `Vec3`, `Entity` (transform/tag accessors), `Input`, `Key`, `Mouse`, `Log` to Lua.

### Lua Script API (for `LuaScriptComponent` scripts)
Each script runs in an isolated `sol::environment`. The global `entity` is a handle to the owning entity.
```lua
function OnCreate()  end        -- called once at runtime start
function OnUpdate(ts) end       -- called every frame; ts = delta time (seconds)
function OnDestroy() end        -- called at runtime stop

-- Available globals: entity, Input, Key, Mouse, Log, Vec3
-- entity:GetTranslation() / SetTranslation(vec3)
-- entity:GetRotation()    / SetRotation(vec3)
-- entity:GetScale()       / SetScale(vec3)
-- entity:GetTag() -> string
-- Input.IsKeyPressed(Key.W), Input.GetMouseX(), etc.
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

## Near-term
- [x] **Lua file watcher** — Background thread polls `std::filesystem::last_write_time` per `.lua` script path; changes are queued thread-safely and drained on the main thread in `OnRuntimeUpdate`, then forwarded to the existing `OnFileChanged` hot-reload logic. Owned by `LuaScriptingBackend` (`engine/src/scripting/file_watcher.h/.cpp`).
- [x] **Circle Collider 2D** — `CircleCollider2DComponent` using Box2D `b2Circle`. Wire into the physics system, serializer, and inspector alongside `BoxCollider2DComponent`.
- [x] **Entity parent-child hierarchy** — `RelationshipComponent` (Parent + Children entt handles) on any entity. World transform computed recursively via `Scene::GetWorldTransform`. Hierarchy panel renders as a tree with drag-and-drop reparenting; relationships serialized via `ParentID` UUID. `entity.cpp` added.

## Medium-term
- [ ] **Sprite animation** — Frame-based `AnimationComponent` cycling UV regions on `SpriteRendererComponent` at a configurable FPS. No new vendor library needed.
- [ ] **Prefab system** — Serialize a single entity (all components) to a `.lprefab` YAML file; instantiate from the editor and from Lua.
- [ ] **Expanded Lua bindings** — Physics raycasts, entity lookup by tag, entity spawn/destroy from scripts, multi-argument `Log` functions.
- [ ] **Audio system** — `AudioEngine` singleton + `AudioSourceComponent`. Candidate library: **miniaudio** (single-header C, no extra submodule overhead).

## 3D Foundation
The engine is structurally 3D-ready: `TransformComponent` uses `glm::vec3`, `EditorCamera` supports perspective navigation, and `SceneCamera` already has a perspective projection type. The following work brings full 3D rendering and physics online.

- [ ] **Renderer3D** — New `engine/renderer/renderer_3d.h/.cpp` system (parallel to `Renderer2D`) for submitting and drawing meshes. Keeps 2D and 3D pipelines independent.
- [ ] **Mesh loading** — Add **cgltf** submodule (`vendor/cgltf`, single C file) for GLTF/GLB import. Wrap in `engine/asset/` as `MeshLoader`. Add to vendor table below.
- [ ] **Mesh & material components** — `MeshComponent` (path to a GLTF asset), `MeshRendererComponent` (mesh + material reference), `MaterialComponent` (albedo color/texture, roughness, metallic).
- [ ] **Basic lighting** — `DirectionalLightComponent`, `PointLightComponent`. Phong shading pass in `Renderer3D` before moving to PBR.
- [ ] **3D physics** — Add **Jolt Physics** submodule (`vendor/jolt`). Introduce a `PhysicsEngine3D` singleton alongside the existing Box2D 2D system. Add `Rigidbody3DComponent`, `BoxCollider3DComponent`, `SphereCollider3DComponent`.

## Longer-term
- [ ] **PBR shading** — Replace Phong with a physically-based rendering pipeline (metallic-roughness model). Requires IBL environment maps.
- [ ] **Shadow mapping** — Directional shadow maps; cascaded shadow maps for large scenes.
- [ ] **Asset hot-reload** — Detect texture, shader, and mesh file changes; reload through `AssetManager` without restarting the editor.
- [ ] **Runtime game export** — Standalone executable with no editor layer; start scene loaded from project config.
- [ ] **Tilemap support** — Tiled `.tmx` loading or a built-in tile editor panel in Weaver.

## Graphics API & Platform Expansion

Start this milestone only after the 3D Foundation is complete and stable. The abstract `RendererAPI` / `RenderCommand` layer is already designed for multi-backend support; platform implementations live in `platform/<api>/`. The window abstraction (`Window::Create()`, `GetNativeWindow()`) is already backend-agnostic. The main cross-cutting concern is the shader pipeline: adding a new API requires either offline compilation to that API's shader format or a cross-compilation step.

Ordered by impact and implementation complexity:

- [ ] **Vulkan** — First non-OpenGL backend. Cross-platform (Windows, Linux, macOS via MoltenVK). Add `platform/vulkan/` implementations. Requires: Vulkan SDK (system install, not a submodule) + **VMA** submodule (`vendor/vma`, Vulkan Memory Allocator) for buffer/image management + **vk-bootstrap** submodule (`vendor/vk-bootstrap`) to reduce init boilerplate. Shaders compiled from GLSL to SPIR-V offline via `glslang` or `shaderc`.
- [ ] **DirectX 12** — Windows-only explicit API, pairs naturally after Vulkan since both are low-overhead and similar in design. No new submodule; uses the Windows SDK. Add `platform/directx12/`. Shaders compiled with `dxc` (HLSL → DXIL).
- [ ] **Win32 window backend** — Native Win32 replacement for GLFW on Windows. Add `platform/win32/` window and input implementations. Removes the GLFW dependency from Windows shipping builds and enables tighter OS integration (raw input, DPI handling, etc.). Pair with DirectX 12 milestone.
- [ ] **DirectX 11** — Compatibility tier for older Windows hardware. Simpler than DX12; consider only if legacy hardware support becomes a requirement.
- [ ] **Metal** — macOS / iOS native API. Vulkan via MoltenVK already covers macOS, so this is low priority. Add `platform/metal/` only if MoltenVK overhead becomes measurable.
- [ ] **Wayland native** — Linux-only. GLFW already supports Wayland via the `GLFW_PLATFORM_WAYLAND` flag; a full native Wayland backend without GLFW is very late-stage.

## Candidate Vendor Libraries (not yet added)
| Library | Submodule path | Purpose |
|---|---|---|
| miniaudio | `vendor/miniaudio` | Audio playback (single-header C) |
| cgltf | `vendor/cgltf` | GLTF/GLB mesh loading (single-header C) |
| Jolt Physics | `vendor/jolt` | 3D physics (C++17, MIT) |
| VMA | `vendor/vma` | Vulkan Memory Allocator (required for Vulkan backend) |
| vk-bootstrap | `vendor/vk-bootstrap` | Vulkan instance/device init boilerplate |

# Your Mission
When generating code, modifying files, or debugging:
1. **Respect separation of concerns:** Never put OpenGL-specific code in the abstract `engine/renderer/` layer — it belongs in `platform/opengl/`.
2. **Use existing libraries:** Use `spdlog` macros for logging, `glm` for all math, EnTT for entity queries.
3. **Editor UI:** Always use `ImGui` for new Weaver panels and windows.
4. **Build system:** Register every new `.cpp` file in the appropriate `CMakeLists.txt`.
5. **Patterns:** Follow existing naming conventions, include order, and member variable style before introducing new patterns.
