# Loom Engine

A 2D + 3D game engine written in **C++20**, built around an abstract OpenGL renderer, Box2D 3.x for 2D physics, Jolt for 3D physics, and a Lua 5.4 scripting layer.

**Weaver** is the accompanying Dear ImGui editor — design scenes visually, tweak components in the inspector, author Lua scripts with live property overrides, and hit Play, all without leaving the tool.

The project ships three distinct targets: the **Loom** engine library, the **Weaver** editor, and **WeaverRuntime** — a lean standalone executable that runs a finished project with no editor or ImGui code linked in.

---

## Features

### Weaver Editor

- **Scene Hierarchy** — entity tree with drag-and-drop reparenting; parent-child world transform composition; right-click context actions (create child, detach, save as prefab, delete)
- **Viewport** — framebuffer-rendered scene with `EditorCamera`; translate / rotate / scale gizmos via ImGuizmo; world / local space toggle; click-to-select mouse picking
- **Tile paint tool** — visual sheet palette inspector + in-viewport cell painter with hover highlight, LMB-drag stroke painting, shift-click to mark a sheet tile as solid for runtime collision
- **Infinite dynamic grid** — perspective-aware fade, configurable snap
- **Component Inspector** — property editors for every built-in component; right-click to remove; "Add Component" menu
- **Content Browser** — project asset file tree; drag textures onto sprite slots; drag `.loom` scenes or `.lprefab` prefabs onto the viewport to open or instantiate
- **Play / Stop** — enter runtime in-editor; physics, scripting, and audio activate on play and are torn down cleanly on stop
- **Project & Scene I/O** — New / Open / Save / Save As for both projects and scenes; "unsaved changes" guard modal; recently opened projects list
- **Project Settings** — configure window title, resolution, and start scene from a dedicated modal; settings round-trip through the `.loomproj` file and are consumed by WeaverRuntime at launch
- **Undo / Redo** — 50-step command history (`Ctrl+Z` / `Ctrl+Shift+Z`); covers entity creation and deletion, component add/remove, gizmo transforms, and every inspector property edit; title-bar dirty indicator is driven by history depth rather than a manual flag
- **Editor camera persistence** — camera position, pitch, and yaw saved per scene and restored exactly on re-open

### Script Property Exposure

Expose typed variables from a Lua script to the Inspector without editing Lua source. Declare a top-level `Properties` table and Weaver discovers the fields automatically:

```lua
Properties = {
    Speed  = 5.0,   -- float   → DragFloat widget
    Lives  = 3,     -- int     → DragInt widget
    Active = true,  -- bool    → Checkbox widget
    Label  = "hero" -- string  → InputText widget
}
```

Each field appears as a live editor widget. Values are saved to the scene file and injected into the script environment before `OnCreate()` runs. In Play mode the inspector displays live runtime values read directly from the running environment.

### Renderer

- **Renderer2D** — batched quads, textures, color tint, tiling factor, lines, circles, signed-distance text (stb_truetype glyph atlas), tilemap rendering
- **Renderer3D** — mesh facade with one draw call per submission; Blinn-Phong lighting with directional and point lights; albedo color + texture + roughness/metallic uniforms (PBR upgrade planned for Phase 5)
- **Shadow mapping** — directional cascaded shadow maps (4 cascades, practical split scheme, 3×3 PCF softening, slope-scale bias, per-cascade sphere-fit + texel-snap stabilization)
- **Particles** — CPU-simulated emitters (point/box/circle shape, world or local space) with lifetime / velocity / gravity ranges and color/size curves; live preview in the editor
- **Sprite animation** — multi-clip `AnimationComponent` (per-clip frames, frame duration, loop); visual spritesheet picker (click cells to add frames); per-frame events dispatched to Lua as `OnAnimationEvent(name)`
- **TextureSpecification** — per-texture filter mode (nearest / linear), wrap mode (repeat / clamp), mipmap generation
- **Cameras** — `OrthographicCamera`, `SceneCamera` for gameplay, perspective `EditorCamera` for the viewport; constant-size billboarded camera icons in the editor

### ECS & Scene

- **EnTT** entity-component system; game objects are lightweight handles over a registry
- Built-in components: `Transform`, `Tag`, `Camera`, `SpriteRenderer`, `MeshRenderer`, `NativeScript`, `LuaScript`, `Rigidbody2D` / `BoxCollider2D` / `CircleCollider2D`, `Rigidbody3D` / `BoxCollider3D` / `SphereCollider3D` / `CapsuleCollider3D`, `DirectionalLight`, `PointLight`, `Animation`, `AudioSource`, `Tilemap`, `Particle`, `Text`
- **Entity hierarchy** — `RelationshipComponent`; world transform computed from local transforms up the parent chain
- **YAML scene serialization** — `.loom` scene files; asset paths stored project-relative for portability; editor camera state persisted per scene
- **Prefab system** — save any entity as `.lprefab`; drag-and-drop instantiation from the content browser; `entity:Instantiate(path)` from Lua
- **Scene transitions** — `Scene.Load("path")` and `Scene.Reload()` from Lua queue a scene change at end-of-frame; works identically in Weaver Play mode and WeaverRuntime

### 2D Physics — Box2D 3.x

- Static, Dynamic, and Kinematic body types; `FixedRotation` constraint
- `BoxCollider2DComponent` and `CircleCollider2DComponent` — density, friction, restitution, and per-shape offset
- **Tilemap colliders** — per-sheet-tile `Solid` flag compiles to a single static body per tilemap with greedy row-merged box fixtures
- **Collision callbacks** — `OnCollisionBegin` / `OnCollisionEnd` dispatched to Lua scripts on both involved entities each physics step
- **Sensor / trigger colliders** — `IsSensor` toggle on any collider; sensors detect overlap without exerting force; dispatches `OnSensorBegin` / `OnSensorEnd` to Lua on both entities
- **Spatial overlap queries** — `Physics.OverlapCircle(center, radius)` and `Physics.OverlapBox(center, half_extents)` return a Lua array of all overlapping entities
- Physics bodies synced back to `TransformComponent` every frame
- Debug wireframe overlay rendered in the editor viewport (boxes, circles, tilemap colliders)

### 3D Physics — Jolt

- Static, Dynamic, and Kinematic body types via `Rigidbody3DComponent`; `FixedRotation` constraint; linear and angular damping
- Colliders: `BoxCollider3D`, `SphereCollider3D`, `CapsuleCollider3D` — density, friction, restitution, sensor flag, per-shape offset
- `PhysicsEngine3D` singleton owns process-wide Jolt state (allocator, factory, job system, layer filters); per-scene `JPH::PhysicsSystem` lives on `Scene`
- **Collision and sensor callbacks** — Jolt worker-thread events queued into a mutex-protected buffer, drained on the main thread, and routed through the same `OnCollisionBegin/End` / `OnSensorBegin/End` Lua callbacks as 2D
- Debug wireframe overlay for boxes (oriented), spheres (three world-axis great circles), and capsules (rings + cylinder edges + hemisphere arcs)

### Scripting — Lua 5.4

- Per-entity isolated `sol::environment`s — scripts never share global state
- **Hot-reload** — file watcher detects `.lua` changes on disk and reloads the script instantly while in Play mode
- **Script property overrides** — declare a `Properties` table; Weaver discovers fields, presents inspector widgets, saves overrides to the scene file, and injects values before `OnCreate()` runs
- Complete entity API: transform, tag, scene management, audio control, 2D and 3D physics body manipulation, animation playback
- Collision, sensor, and per-frame animation callbacks delivered directly to the owning script environment
- `Physics.Raycast`, `Physics.OverlapCircle`, `Physics.OverlapBox` world queries from Lua
- `Scene.Load` / `Scene.Reload` for Lua-driven level transitions

### Audio — miniaudio

- `AudioEngine` singleton with miniaudio backend
- `AudioSourceComponent` — asset path, volume, pitch, pan, loop, autoplay
- Autoplay fires on `OnRuntimeStart`; all sources stopped and cleaned up on `OnRuntimeStop`
- Full runtime control from Lua: `PlayAudio`, `StopAudio`, `IsAudioPlaying`, `SetVolume`, `SetPitch`

### Asset Management

- `AssetManager` — centralized cache for textures, shaders, meshes (GLTF/GLB via cgltf), and fonts (TTF glyph atlas via stb_truetype); assets loaded once and shared across the scene
- `FontManager` — editor UI font atlas with named `FontType` slots (`Small`, `Medium`, `Large`, `Monospace`, …); merges Inter + Noto Sans Thai for Latin/Thai text, Roboto Mono for monospace
- **Hot-reload** — `FileWatcher` detects texture and shader changes on disk; GPU resources patched in-place during Play mode
- Asset paths normalized to project-relative on save, resolved to absolute at runtime via `Project::GetAssetFileSystemPath`

### WeaverRuntime

- Standalone executable that loads a `.loomproj` file and runs the game at full speed
- No editor, ImGui, or file-dialog code linked in — pure engine + Lua
- Reads `WindowTitle`, `WindowWidth`, `WindowHeight` from the project config before constructing the window
- Handles Lua-driven `Scene.Load` / `Scene.Reload` transitions each frame
- All asset paths and engine resources resolve correctly relative to the executable directory

---

## Current Status

Phases 1 through 4 are **complete**. The engine is 2D + 3D feature-complete with shadow-mapped cascades, a working standalone runtime, and a polished editor. Active work is **Phase 5 — Advanced Rendering**.

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 1 | 2D Feature Complete — ECS, physics, scripting, audio, prefabs, animation, collision/sensor callbacks, spatial queries, scene transitions, script property exposure | ✅ Complete |
| Phase 1.5 | Project System Hardening — schema versioning, error modals, runtime window config, Project Settings modal, recently opened projects | ✅ Complete |
| Phase 2 | WeaverRuntime standalone executable | ✅ Complete |
| Phase 3 | Editor & Tools Polish — undo/redo (Command Pattern), text/HUD rendering, tilemap (paint tool + per-tile collision), particle system, custom gizmos, animation polish (multi-clip + events + visual picker) | ✅ Complete |
| Phase 4 | 3D Foundation — GLTF mesh loading (cgltf), `Renderer3D`, Blinn-Phong lighting (directional + point), Jolt 3D physics with Box/Sphere/Capsule colliders + collision events | ✅ Complete |
| Phase 5 | Advanced Rendering — directional cascaded shadow maps ✅ done; PBR shading + IBL ⏳ in progress | ⏳ In progress |
| Phase 6 | Graphics API Expansion — Vulkan and DirectX 12 backends | 🔲 Planned |

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

## Running

### Weaver Editor

After a successful build, launch Weaver from the output directory:

```bash
./build/debug/bin/Weaver
```

On first launch, create or open a project via **File > New Project** or **File > Open Project**. All scene assets and scripts live inside the project's asset directory.

### WeaverRuntime

Pass a `.loomproj` file as the first argument:

```bash
./build/debug/bin/WeaverRuntime path/to/MyGame.loomproj
```

The runtime reads window title and resolution from the project config and launches directly into the configured start scene.

---

## Project Structure

```
engine/           Core engine library (Loom)
  include/        Public headers, included as <loom/...>
  src/
    core/         Application loop, events, input, logging
    renderer/     Renderer2D, Renderer3D, shadow CSM, MeshAsset, FontAsset,
                  shaders, textures, framebuffers, cameras
    scene/        ECS (EnTT), components, scene serialization, prefabs
    scripting/    ScriptingEngine facade and Lua backend
    audio/        AudioEngine (miniaudio)
    physics/      PhysicsEngine3D (Jolt) — process-wide init
    platform/     GLFW windowing, OpenGL driver implementations
    asset/        AssetManager (texture / shader / mesh / font cache, hot-reload),
                  FontManager (editor UI font atlas)
    project/      Project and ProjectSerializer
weaver/           Editor application (Weaver)
  src/
    panels/       Viewport (gizmo + tile paint), scene hierarchy, content browser, toolbar
    editor/       Scene and project I/O managers, undo/redo command history
    scripts/      Native C++ script examples
weaver_runtime/   Standalone runtime executable (WeaverRuntime)
sandbox/          Example project demonstrating engine usage
resources/        Engine shaders, fonts, and icons
vendor/           Third-party libraries (Git submodules)
cmake/            CMake helper modules
```

---

## Scripting with Lua

Attach a **Lua Script** component to any entity in Weaver, point it at a `.lua` file inside the project asset directory (drag from the Content Browser or use the browse button), then hit **Play**.

Scripts run in isolated environments and support hot-reload — save the file on disk and Loom reloads it immediately while the scene is running.

### Script Properties

Declare a top-level `Properties` table to expose editable fields to the Weaver inspector:

```lua
Properties = {
    Speed     = 5.0,
    JumpForce = 8.0,
    MaxHealth = 100,
}

function OnUpdate(ts)
    -- Speed, JumpForce, MaxHealth are injected as globals from the inspector values
    if Input.IsKeyPressed(Key.D) then entity:ApplyForce(Vec2(Speed, 0)) end
    if Input.IsKeyPressed(Key.A) then entity:ApplyForce(Vec2(-Speed, 0)) end
end
```

Property values set in the inspector are saved to the scene file and restored on every run without modifying the script.

### Lifecycle hooks

```lua
function OnCreate()              end  -- called once when the scene starts
function OnUpdate(ts)            end  -- called every frame; ts = delta time in seconds
function OnDestroy()             end  -- called when the scene stops

function OnCollisionBegin(other) end  -- solid collider first contact (2D or 3D)
function OnCollisionEnd(other)   end  -- solid collider separation
function OnSensorBegin(other)    end  -- entity enters a sensor / trigger area
function OnSensorEnd(other)      end  -- entity exits a sensor / trigger area

function OnAnimationEvent(name)  end  -- fires when the active AnimationClip enters
                                      -- a frame tagged with this event name
```

All collision and sensor callbacks receive `other` — an entity handle to the other participant. `OnAnimationEvent` receives the event's `Name` string, configured per-frame in the inspector.

### Globals

| Global    | Description                                           |
|-----------|-------------------------------------------------------|
| `entity`  | Handle to the owning entity                           |
| `Vec2`    | 2-component vector with `+`, `-`, `*` operators       |
| `Vec3`    | 3-component vector with `+`, `-`, `*` operators       |
| `Input`   | Keyboard and mouse query functions                    |
| `Key`     | Key code constants (`Key.W`, `Key.Space`, …)          |
| `Mouse`   | Mouse button constants (`Mouse.Left`, …)              |
| `Log`     | Engine logging (`Log.Info`, `Log.Warn`, `Log.Error`)  |
| `Physics` | World-level physics queries                           |
| `Scene`   | Scene transition control                              |

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

### Entity API — 2D Physics body *(requires Rigidbody2DComponent)*

```lua
entity:SetLinearVelocity(vec2)
entity:GetLinearVelocity()       -- Vec2
entity:ApplyForce(vec2)
entity:ApplyImpulse(vec2)
```

### Entity API — 3D Physics body *(requires Rigidbody3DComponent)*

```lua
entity:SetLinearVelocity3D(vec3)
entity:GetLinearVelocity3D()     -- Vec3
entity:ApplyForce3D(vec3)
entity:ApplyImpulse3D(vec3)
```

### Entity API — Animation *(requires AnimationComponent)*

```lua
entity:PlayAnimation(name)       -- switch to a named clip and start playing
entity:StopAnimation()           -- pause at the current frame
entity:SetAnimationFrame(n)      -- jump to a specific frame (does NOT fire events)
entity:GetAnimationFrame()       -- int
entity:IsAnimationPlaying()      -- bool
entity:GetCurrentAnimation()     -- string  (name of the active clip)
```

### Physics queries

```lua
Physics.Raycast(origin_vec3, direction_vec3, distance)
-- returns { hit: bool, point: Vec3, normal: Vec3, entity: entity }

Physics.OverlapCircle(center_vec2, radius)
-- returns array of overlapping entities

Physics.OverlapBox(center_vec2, half_extents_vec2)
-- returns array of overlapping entities
```

### Scene transitions

```lua
Scene.Load("scenes/level2.loom")  -- queue a scene change (path relative to asset dir)
Scene.Reload()                    -- restart the current scene from its last saved state
```

Transitions are queued and applied at the end of the frame — safe to call from any callback.

### Example — platformer movement

```lua
Properties = { Speed = 5.0, JumpForce = 8.0 }

function OnUpdate(ts)
    if Input.IsKeyPressed(Key.D) then entity:ApplyForce(Vec2( Speed, 0)) end
    if Input.IsKeyPressed(Key.A) then entity:ApplyForce(Vec2(-Speed, 0)) end
    if Input.IsKeyPressed(Key.Space) then
        entity:ApplyImpulse(Vec2(0, JumpForce))
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

### Example — scene transition on trigger

```lua
function OnSensorBegin(other)
    if other:GetTag() == "Player" then
        Scene.Load("scenes/level2.loom")
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

### Example — animation events

```lua
function OnCreate()
    entity:PlayAnimation("Run")
end

-- Mark frames in the inspector: frame 2 -> "footstep_left",
-- frame 6 -> "footstep_right". This callback fires on each entry.
function OnAnimationEvent(name)
    if name == "footstep_left" or name == "footstep_right" then
        entity:PlayAudio()
    end
end
```

---

## Third-Party Libraries

All libraries are included as Git submodules under `vendor/`.

| Library         | Purpose                              |
|-----------------|--------------------------------------|
| EnTT            | Entity-Component-System              |
| GLFW            | Window and input                     |
| GLAD            | OpenGL loader                        |
| Dear ImGui      | Editor UI                            |
| ImGuiFileDialog | In-process file dialogs              |
| ImGuizmo        | Transform gizmos (T/R/S)             |
| GLM             | Math                                 |
| spdlog          | Logging                              |
| stb_image       | Image loading                        |
| stb_truetype    | Font glyph atlas                     |
| cgltf           | GLTF / GLB mesh loading              |
| Box2D 3.x       | 2D physics                           |
| Jolt Physics    | 3D physics                           |
| yaml-cpp        | Scene serialization                  |
| Lua 5.4         | Scripting VM                         |
| sol2 3.5.0      | Lua C++ bindings                     |
| miniaudio       | Audio playback                       |
