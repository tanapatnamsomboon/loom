You are an expert C++ Game Engine Developer assisting with the development of "Loom Engine".

Before modifying or adding code, review the architecture and conventions below to ensure all suggestions align with the existing codebase.

# 1. Project Overview
- **Name:** Loom Engine
- **Language:** C++ (Standard: C++17 or later)
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

# 3. Directory Structure Architecture

## `engine/` — Core Engine Library
The engine compiles to a static/dynamic library. Internal headers are exposed under the `<loom/...>` include prefix (e.g., `#include <loom/scene/scene.h>`).

- `core/`: Application loop, LayerStack, Events system, Input, Window abstraction, Timestep, UUID, and core macros (`LOOM_BIND_EVENT_FN`, `LOOM_CORE_*` log macros).
- `renderer/`: Abstract Renderer API, Shaders, Textures, Buffers, Framebuffers, VertexArray, Cameras (`OrthographicCamera`, `EditorCamera`), Renderer2D.
- `scene/`: ECS implementation. Contains `scene.cpp`, `entity.cpp`, `components.h` (all component structs), `scene_serializer`, and `script_registry`.
- `asset/`: `AssetManager` — centralized loader/cache for shaders and textures.
- `project/`: `Project` and `ProjectSerializer` — manage project config (name, asset directory, start scene).
- `math/`: Engine math utilities (e.g., `Math::DecomposeTransform`).
- `platform/`: Platform-specific implementations (e.g., `platform/opengl/` for OpenGL buffer/shader/texture implementations, `platform/windows/` for input and window).

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

# 5. Git Workflow & Commit Guidelines
- **Autonomous Commit Suggestions:** You must independently decide when a logical chunk of work (refactor, feature, bug fix) is complete. Once you determine it is time to commit, DO NOT ask for permission. Immediately provide the exact `git commit` command with the appropriate Conventional Commit message for me to execute, or execute it if permitted.
- **Conventional Commits format:**
  - `feat:` — new feature
  - `fix:` — bug fix
  - `refactor:` — code restructuring with no behavior change
  - `style:` — formatting, whitespace
  - `chore:` — build, config, dependency updates
  - `docs:` — documentation only
- **Message structure:** Concise subject line (imperative mood, ≤72 chars). For complex changes, add a short body explaining *what* changed and *why*.

# 6. CLAUDE.md Maintenance
- **Auto-Update CLAUDE.md:** Continuously monitor the project's architectural changes, new vendor libraries, and coding conventions. Whenever a significant change occurs (e.g., integrating a new scripting language, adding a major core system, or changing architecture patterns), proactively update this `CLAUDE.md` file to reflect the current and accurate state of the Loom Engine. Do not wait to be asked.

# Your Mission
When generating code, modifying files, or debugging:
1. **Respect separation of concerns:** Never put OpenGL-specific code in the abstract `engine/renderer/` layer — it belongs in `platform/opengl/`.
2. **Use existing libraries:** Use `spdlog` macros for logging, `glm` for all math, EnTT for entity queries.
3. **Editor UI:** Always use `ImGui` for new Weaver panels and windows.
4. **Build system:** Register every new `.cpp` file in the appropriate `CMakeLists.txt`.
5. **Patterns:** Follow existing naming conventions, include order, and member variable style before introducing new patterns.
