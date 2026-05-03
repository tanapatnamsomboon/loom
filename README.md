# Loom Engine

A 2D game engine written in C++20 with OpenGL rendering, Box2D physics, and Lua scripting. Includes **Weaver**, an ImGui-based editor for building and iterating on scenes.

## Prerequisites

- CMake 3.20 or later
- Ninja build system
- C++20 compiler: MSVC 2022 (Windows), GCC 12+, or Clang 14+
- Git

## Getting Started

Clone the repository and initialize all vendor submodules:

```
git clone https://github.com/tanapatnamsomboon/loom
cd loom
git submodule update --init --recursive
```

## Building

Loom uses CMake Presets. Configure and build with a single command pair:

**Debug**
```
cmake --preset debug
cmake --build --preset debug
```

**Release**
```
cmake --preset release
cmake --build --preset release
```

Binaries are written to `build/<preset>/bin/`. Engine resources are copied there automatically.

To build a single target:

```
cmake --build --preset debug-weaver   # editor only
cmake --build --preset debug-loom     # engine library only
```

## Running the Editor

After a successful build, launch Weaver from the output directory:

```
./build/debug/bin/Weaver
```

On the first launch, create or open a project via **File > New Project** or **File > Open Project**. All scene assets and scripts live inside the project's asset directory.

## Project Structure

```
engine/         Core engine library (Loom)
  include/      Public headers, included as <loom/...>
  src/
    core/       Application loop, events, input, logging
    renderer/   Renderer2D, shaders, textures, framebuffers, cameras
    scene/      ECS (EnTT), components, scene serialization
    scripting/  ScriptingEngine facade and Lua backend
    platform/   GLFW windowing, OpenGL driver implementations
    asset/      AssetManager (texture and shader cache)
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

## Scripting with Lua

Attach a **Lua Script** component to any entity in Weaver, then point it at a `.lua` file inside the project's asset directory (drag from the Content Browser or use the `...` browse button).

Scripts run in isolated environments. Three lifecycle hooks are available:

```lua
function OnCreate()      end  -- called once when the scene starts
function OnUpdate(ts)    end  -- called every frame; ts is delta time in seconds
function OnDestroy()     end  -- called when the scene stops
```

### Available globals

| Global  | Description                                          |
|---------|------------------------------------------------------|
| `entity`| Handle to the owning entity                          |
| `Input` | Keyboard and mouse query functions                   |
| `Key`   | Key code constants (`Key.W`, `Key.Space`, ...)       |
| `Mouse` | Mouse button constants (`Mouse.Left`, ...)           |
| `Log`   | Engine logging (`Log.Info`, `Log.Warn`, `Log.Error`) |
| `Vec3`  | 3-component vector with `+`, `-`, `*` operators      |

### Entity API

```lua
entity:GetTranslation()       -- returns Vec3
entity:SetTranslation(vec3)
entity:GetRotation()          -- returns Vec3 (Euler angles in radians)
entity:SetRotation(vec3)
entity:GetScale()             -- returns Vec3
entity:SetScale(vec3)
entity:GetTag()               -- returns string
```

### Example

```lua
local speed = 5.0

function OnUpdate(ts)
    local pos = entity:GetTranslation()
    if Input.IsKeyPressed(Key.D) then pos.x = pos.x + speed * ts end
    if Input.IsKeyPressed(Key.A) then pos.x = pos.x - speed * ts end
    entity:SetTranslation(pos)
end
```

Scripts can be hot-reloaded during Play mode by clicking **Reload** in the inspector.

## Third-Party Libraries

All libraries are included as Git submodules under `vendor/`.

| Library                      | Purpose                      |
|------------------------------|------------------------------|
| EnTT                         | Entity-Component-System      |
| GLFW                         | Window and input             |
| GLAD                         | OpenGL loader                |
| Dear ImGui                   | Editor UI                    |
| ImGuizmo                     | Transform gizmos             |
| GLM                          | Math                         |
| spdlog                       | Logging                      |
| stb_image                    | Image loading                |
| Box2D 3.x                    | 2D physics                   |
| yaml-cpp                     | Scene serialization          |
| nativefiledialog-extended    | Native file dialogs          |
| Lua 5.4                      | Scripting VM                 |
| sol2 3.5.0                   | Lua C++ bindings             |
