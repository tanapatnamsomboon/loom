# Loom Engine

A 2D game engine written in **C++20**, built around an abstract OpenGL renderer, Box2D 3.x physics, and a Lua 5.4 scripting layer.

**Weaver** is the accompanying Dear ImGui editor — design scenes visually, tweak components in the inspector, author scripts, and hit Play, all without leaving the tool.

The codebase is structured as three distinct targets: the **Loom** engine library, the **Weaver** editor, and an upcoming **WeaverRuntime** that ships a finished project as a lean standalone executable with no editor code linked in.

---

## Features

### Weaver Editor

- **Scene Hierarchy** — entity tree with drag-and-drop reparenting; parent-child world transform composition; right-click context actions (create child, detach, save as prefab, delete)
- **Viewport** — framebuffer-rendered scene with `EditorCamera`; translate / rotate / scale gizmos via ImGuizmo; click-to-select mouse picking
- **Infinite dynamic grid** — perspective-aware fade, configurable snap
- **Component Inspector** — property editors for every built-in component; right-click to remove; "Add Component" menu
- **Content Browser** — project asset file tree; drag textures onto sprite slots; drag `.loom` scenes or `.lprefab` prefabs onto the viewport to open or instantiate
- **Play / Stop** — enter runtime in-editor; physics, scripting, and audio activate on play and are torn down cleanly on stop
- **Project & Scene I/O** — New / Open / Save / Save As for both projects and scenes; "unsaved changes" guard modal

### Renderer

- **Renderer2D** — batched quad rendering with color tint, texture, and tiling factor
- **TextureSpecification** — per-texture filter mode (nearest / linear), wrap mode (repeat / clamp), mipmap generation
- **Sprite animation** — frame-based `AnimationComponent`; configurable FPS, loop toggle; spritesheet auto-fill helper (cell size, start row/col, frame count)
- **Cameras** — `OrthographicCamera` for gameplay, perspective `EditorCamera` for the viewport; constant-size billboarded camera icons in the editor

### ECS & Scene

- **EnTT** entity-component system; game objects are lightweight handles over a registry
- Built-in components: `Transform`, `Tag`, `Camera`, `SpriteRenderer`, `NativeScript`, `LuaScript`, `Rigidbody2D`, `BoxCollider2D`, `CircleCollider2D`, `Animation`, `AudioSource`
- **Entity hierarchy** — `RelationshipComponent`; world transform computed from local transforms up the parent chain
- **YAML scene serialization** — `.loom` scene files; asset paths stored project-relative for portability
- **Prefab system** — save any entity as `.lprefab`; drag-and-drop instantiation from the content browser; `entity:Instantiate(path)` from Lua

### Physics — Box2D 3.x

- Static, Dynamic, and Kinematic body types; `FixedRotation` constraint
- `BoxCollider2DComponent` and `CircleCollider2DComponent` — density, friction, restitution, and per-shape offset
- **Collision callbacks** — `OnCollisionBegin` / `OnCollisionEnd` dispatched to Lua scripts on both involved entities each physics step
- **Sensor / trigger colliders** — `IsSensor` toggle on any collider; sensors detect overlap without exerting physical force; dispatches `OnSensorBegin` / `OnSensorEnd` to Lua on both entities
- Physics bodies synced back to `TransformComponent` every frame
- Debug wireframe overlay (boxes and circles) rendered in the editor viewport

### Scripting — Lua 5.4

- Per-entity isolated `sol::environment`s — scripts never share global state
- **Hot-reload** — file watcher detects `.lua` changes on disk and reloads the script instantly while in Play mode
- Complete entity API: transform, tag, scene management, audio control, physics body manipulation
- Collision and sensor callbacks delivered directly to the owning script environment
- `Physics.Raycast` world query from Lua

### Audio — miniaudio

- `AudioEngine` singleton with miniaudio backend
- `AudioSourceComponent` — asset path, volume, pitch, pan, loop, autoplay
- Autoplay fires on `OnRuntimeStart`; all sources stopped and cleaned up on `OnRuntimeStop`
- Full runtime control from Lua: `PlayAudio`, `StopAudio`, `IsAudioPlaying`, `SetVolume`, `SetPitch`

### Asset Management

- `AssetManager` — centralized texture and shader cache; assets loaded once and shared across the scene
- **Hot-reload** — `FileWatcher` detects texture and shader changes on disk; GPU resources patched in-place during Play mode
- Asset paths normalized to project-relative on save, resolved to absolute at runtime

---

## Current Status

Loom is in active development targeting **Phase 1: 2D Feature Complete** — everything a 2D game needs before the standalone runtime ships.

### Phase 1 — 2D Feature Complete

| Feature | Status |
|---|---|
| Core engine (ECS, Renderer2D, events, input) | ✅ Done |
| Weaver editor (viewport, hierarchy, inspector, content browser) | ✅ Done |
| Box2D physics integration | ✅ Done |
| Lua scripting with hot-reload | ✅ Done |
| Audio system with runtime Lua control | ✅ Done |
| Prefab system | ✅ Done |
| Sprite animation + spritesheet helper | ✅ Done |
| Asset hot-reload | ✅ Done |
| Physics collision callbacks (`OnCollisionBegin` / `OnCollisionEnd`) | ✅ Done |
| Sensor / trigger colliders (`OnSensorBegin` / `OnSensorEnd`) | ✅ Done |
| Spatial overlap queries (`Physics.OverlapCircle` / `Physics.OverlapBox`) | 🔲 Planned |
| Scene transitions (`Scene.Load` / `Scene.Reload` from Lua) | 🔲 Planned |

### Phase 2 — WeaverRuntime

A standalone runtime executable (`WeaverRuntime`) that loads a project file from disk and runs it at full speed with no editor or ImGui code linked in. Projects built in Weaver become shippable games.

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
function OnCreate()              end  -- called once when the scene starts
function OnUpdate(ts)            end  -- called every frame; ts = delta time in seconds
function OnDestroy()             end  -- called when the scene stops

function OnCollisionBegin(other) end  -- solid collider first contact
function OnCollisionEnd(other)   end  -- solid collider separation
function OnSensorBegin(other)    end  -- entity enters a sensor / trigger area
function OnSensorEnd(other)      end  -- entity exits a sensor / trigger area
```

All collision and sensor callbacks receive `other` — an entity handle to the other participant.

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
entity:GetTranslation()          -- Vec3
entity:SetTranslation(vec3)
entity:GetRotation()             -- Vec3 (Euler, radians)
entity:SetRotation(vec3)
entity:GetScale()                -- Vec3
entity:SetScale(vec3)
entity:GetTag()                  -- string

entity:FindByTag(tag)            -- entity
entity:Spawn()                   -- entity  (new blank entity in same scene)
entity:Destroy()
entity:Instantiate(path)         -- entity  (spawns a .lprefab file)
```

### Entity API — Audio *(requires AudioSourceComponent)*

```lua
entity:PlayAudio()
entity:StopAudio()
entity:IsAudioPlaying()          -- bool
entity:SetVolume(v)              -- 0.0 – 1.0
entity:SetPitch(p)               -- 0.1 – 4.0
```

### Entity API — Physics body *(requires Rigidbody2DComponent)*

```lua
entity:SetLinearVelocity(vec2)
entity:GetLinearVelocity()       -- Vec2
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

### Example — sensor / trigger zone

```lua
function OnSensorBegin(other)
    Log.Info(other:GetTag() .. " entered the zone")
end

function OnSensorEnd(other)
    Log.Info(other:GetTag() .. " left the zone")
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

| Library                   | Purpose                      |
|---------------------------|------------------------------|
| EnTT                      | Entity-Component-System      |
| GLFW                      | Window and input             |
| GLAD                      | OpenGL loader                |
| Dear ImGui                | Editor UI                    |
| ImGuizmo                  | Transform gizmos             |
| GLM                       | Math                         |
| spdlog                    | Logging                      |
| stb_image                 | Image loading                |
| Box2D 3.x                 | 2D physics                   |
| yaml-cpp                  | Scene serialization          |
| nativefiledialog-extended | Native file dialogs          |
| Lua 5.4                   | Scripting VM                 |
| sol2 3.5.0                | Lua C++ bindings             |
| miniaudio                 | Audio playback               |
