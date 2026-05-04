# Loom Engine

A 2D game engine written in C++20, built around OpenGL rendering, Box2D 3.x physics, and Lua scripting.  
**Weaver** is the accompanying ImGui-based editor — create scenes, tweak components, write scripts, and hit Play, all without leaving the tool.

The engine is architected for a clean separation between the engine library (`Loom`), the editor (`Weaver`), and an upcoming standalone runtime (`WeaverRuntime`) that lets finished projects ship as self-contained executables.

---

## Features

### Editor — Weaver

- **Scene Hierarchy** — tree view of all entities; drag-and-drop reparenting; parent-child world transform composition
- **Viewport** — framebuffer-rendered scene with `EditorCamera`; transform gizmos (translate / rotate / scale) via ImGuizmo; click-to-select mouse picking
- **Infinite dynamic grid** — configurable snap, fades with zoom
- **Component Inspector** — property editors for all built-in components; per-component context menu to add or remove
- **Content Browser** — file tree of the project asset directory; drag textures onto sprite slots; drag `.loom` scenes or `.lprefab` prefabs onto the viewport to open or instantiate
- **Play / Stop mode** — enter runtime in-editor; all physics, scripts, and audio activate on play and are torn down cleanly on stop
- **Project & Scene I/O** — New / Open / Save / Save As for both projects and scenes; "unsaved changes" guard modal

### Renderer

- **Renderer2D** — batched quad rendering; sprite color, texture, and tiling factor
- **TextureSpecification** — per-texture filter mode (nearest / linear), wrap mode, and mipmap generation
- **Sprite animation** — frame-based `AnimationComponent`; configurable FPS, loop toggle; spritesheet auto-fill helper (start row/col, cell size, frame count)
- **Cameras** — `OrthographicCamera` for gameplay, `EditorCamera` (perspective) for the Weaver viewport

### ECS & Scene

- **EnTT** entity-component system; all game objects are lightweight entity handles
- Built-in components: `Transform`, `Tag`, `Camera`, `SpriteRenderer`, `NativeScript`, `LuaScript`, `Rigidbody2D`, `BoxCollider2D`, `CircleCollider2D`, `Animation`, `AudioSource`
- **Entity hierarchy** — `RelationshipComponent`; world transform computed from local transforms up the parent chain
- **YAML scene serialization** — `.loom` scene files; `.lprefab` single-entity prefab files
- **Prefab system** — save any entity as a prefab from the editor; drag-and-drop to instantiate; `entity:Instantiate(path)` from Lua

### Physics — Box2D 3.x

- Static, Dynamic, and Kinematic body types
- `BoxCollider2DComponent` and `CircleCollider2DComponent` with density, friction, restitution, and offset
- `FixedRotation` constraint
- Physics bodies synced back to `TransformComponent` each frame

### Scripting — Lua 5.4

- Per-entity isolated `sol::environment`s; scripts never share global state
- **Hot-reload** — the file watcher detects `.lua` changes on disk and reloads the script instantly during Play mode
- Full entity API for transform, tag, scene management, audio control, and physics body manipulation (see Scripting section)
- `Physics.Raycast` from Lua

### Audio — miniaudio

- `AudioEngine` singleton with miniaudio backend
- `AudioSourceComponent` — asset path, volume, pitch, pan, loop, autoplay
- Autoplay fires on `OnRuntimeStart`; all sources cleaned up on `OnRuntimeStop`
- Runtime control from Lua: `PlayAudio`, `StopAudio`, `IsAudioPlaying`, `SetVolume`, `SetPitch`

### Asset Management

- `AssetManager` — centralized texture and shader cache; assets loaded once and reused
- **Hot-reload** — `FileWatcher` detects texture and shader changes on disk; GPU resources updated in-place during Play mode
- Asset paths normalized to project-relative paths for cross-machine portability

---

## Current Status

Loom is in active development, working through its **Phase 1: 2D Feature Complete** milestone. The goal is to ship everything a 2D game needs before building the standalone runtime.

**Completed:**
- Core engine architecture (ECS, Renderer2D, events, input)
- Full Weaver editor (viewport, hierarchy, inspector, content browser)
- Box2D physics integration
- Lua scripting with hot-reload
- Audio system with runtime Lua control
- Prefab system
- Sprite animation
- Asset hot-reload

**In progress (Phase 1):**
- Physics scripting — collision callbacks (`OnCollisionBegin` / `OnCollisionEnd`), sensor/trigger colliders, spatial overlap queries
- Scene transitions (`Scene.Load` / `Scene.Reload` from Lua)

**Next (Phase 2):**
- `WeaverRuntime` — a standalone executable that loads and runs a Weaver project without any editor code linked in

---

## Prerequisites

- CMake 3.20 or later
- Ninja build system
- C++20 compiler: MSVC 2022 (Windows), GCC 12+, or Clang 14+
- Git

---

## Getting Started

Clone the repository and initialize all vendor submodules:

```bash
git clone https://github.com/tanapatnamsomboon/loom
cd loom
git submodule update --init --recursive
```

---

## Building

Loom uses CMake Presets. Configure and build with a single command pair:

**Debug**
```bash
cmake --preset debug
cmake --build --preset debug
```

**Release**
```bash
cmake --preset release
cmake --build --preset release
```

Binaries are written to `build/<preset>/bin/`. Engine resources are copied there automatically.

To build a single target:

```bash
cmake --build --preset debug-weaver   # editor only
cmake --build --preset debug-loom     # engine library only
```

---

## Running the Editor

After a successful build, launch Weaver from the output directory:

```bash
./build/debug/bin/Weaver
```

On first launch, create or open a project via **File > New Project** or **File > Open Project**. All scene assets and scripts live inside the project's asset directory.

---

## Project Structure

```
engine/         Core engine library (Loom)
  include/      Public headers, included as <loom/...>
  src/
    core/       Application loop, events, input, logging
    renderer/   Renderer2D, shaders, textures, framebuffers, cameras
    scene/      ECS (EnTT), components, scene serialization, prefabs
    scripting/  ScriptingEngine facade and Lua backend
    audio/      AudioEngine (miniaudio)
    platform/   GLFW windowing, OpenGL driver implementations
    asset/      AssetManager (texture and shader cache, hot-reload)
    project/    Project and ProjectSerializer
weaver/         Editor application (Weaver)
  src/
    panels/     Viewport, scene hierarchy, content browser, toolbar
    editor/     Scene and project I/O managers
    scripts/    Native C++ script examples
sandbox/        Example project demonstrating engine usage
resources/      Engine shaders, fonts, and icons
vendor/         Third-party libraries (Git submodules)
cmake/          CMake helper modules
```

---

## Scripting with Lua

Attach a **Lua Script** component to any entity in Weaver, point it at a `.lua` file inside the project asset directory (drag from the Content Browser or use the browse button), then hit **Play**.

Scripts run in isolated environments and support hot-reload — save the file on disk and Loom reloads it immediately while the scene is running.

### Lifecycle hooks

```lua
function OnCreate()    end  -- called once when the scene starts
function OnUpdate(ts)  end  -- called every frame; ts is delta time in seconds
function OnDestroy()   end  -- called when the scene stops
```

### Globals

| Global    | Description                                          |
|-----------|------------------------------------------------------|
| `entity`  | Handle to the owning entity                          |
| `Vec2`    | 2-component vector with `+`, `-`, `*` operators      |
| `Vec3`    | 3-component vector with `+`, `-`, `*` operators      |
| `Input`   | Keyboard and mouse query functions                   |
| `Key`     | Key code constants (`Key.W`, `Key.Space`, …)         |
| `Mouse`   | Mouse button constants (`Mouse.Left`, …)             |
| `Log`     | Engine logging (`Log.Info`, `Log.Warn`, `Log.Error`) |
| `Physics` | World-level physics queries                          |

### Entity API — Transform & Scene

```lua
entity:GetTranslation()             -- Vec3
entity:SetTranslation(vec3)
entity:GetRotation()                -- Vec3 (Euler, radians)
entity:SetRotation(vec3)
entity:GetScale()                   -- Vec3
entity:SetScale(vec3)
entity:GetTag()                     -- string

entity:FindByTag(tag)               -- entity
entity:Spawn()                      -- entity  (new blank entity in same scene)
entity:Destroy()
entity:Instantiate(path)            -- entity  (spawns a .lprefab file)
```

### Entity API — Audio *(requires AudioSourceComponent)*

```lua
entity:PlayAudio()
entity:StopAudio()
entity:IsAudioPlaying()             -- bool
entity:SetVolume(v)                 -- 0.0 – 1.0
entity:SetPitch(p)                  -- 0.1 – 4.0
```

### Entity API — Physics body *(requires Rigidbody2DComponent)*

```lua
entity:SetLinearVelocity(vec2)
entity:GetLinearVelocity()          -- Vec2
entity:ApplyForce(vec2)
entity:ApplyImpulse(vec2)
```

### Physics queries

```lua
Physics.Raycast(origin_vec3, direction_vec3, distance)
-- returns { hit: bool, point: Vec3, normal: Vec3, entity: entity }
```

### Example — platformer movement

```lua
local speed = 5.0
local jump  = 8.0

function OnUpdate(ts)
    if Input.IsKeyPressed(Key.D) then entity:ApplyForce(Vec2( speed, 0)) end
    if Input.IsKeyPressed(Key.A) then entity:ApplyForce(Vec2(-speed, 0)) end
    if Input.IsKeyPressed(Key.Space) then
        entity:ApplyImpulse(Vec2(0, jump))
    end
end
```

### Example — audio trigger

```lua
function OnCreate()
    entity:SetPitch(1.2)
    entity:PlayAudio()
end

function OnDestroy()
    entity:StopAudio()
end
```

---

## Third-Party Libraries

All libraries are included as Git submodules under `vendor/`.

| Library                    | Purpose                      |
|----------------------------|------------------------------|
| EnTT                       | Entity-Component-System      |
| GLFW                       | Window and input             |
| GLAD                       | OpenGL loader                |
| Dear ImGui                 | Editor UI                    |
| ImGuizmo                   | Transform gizmos             |
| GLM                        | Math                         |
| spdlog                     | Logging                      |
| stb_image                  | Image loading                |
| Box2D 3.x                  | 2D physics                   |
| yaml-cpp                   | Scene serialization          |
| nativefiledialog-extended  | Native file dialogs          |
| Lua 5.4                    | Scripting VM                 |
| sol2 3.5.0                 | Lua C++ bindings             |
| miniaudio                  | Audio playback               |
