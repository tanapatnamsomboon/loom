# Loom Engine — Project Log

Dedicated tracker for project history and forward planning. AI instructions live in `CLAUDE.md`.

---

# 1. Engineering Log

A chronological ledger of multi-session debugging efforts that resulted in significant architectural pivots, and notable manual implementations worth recording for future context. New entries go at the top. Keep entries terse — one paragraph, no rehashed code.

- **2026-05-13 — Manually implemented a custom Font Manager for the editor UI.** Pulled all font loading out of `ImGuiLayer::OnAttach` and into a dedicated `Loom::FontManager` singleton (`engine/include/loom/asset/font_manager.h` + `engine/src/asset/font_manager.cpp`). `FontType` enum covers the editor's typography slots: `Small`, `Medium`, `MediumBold`, `Large`, `LargeBold`, and `Monospace`. Public static API: `Init(dpi_scale)` builds the atlas (called once during ImGui setup), `Get(FontType)` returns the `ImFont*`, and `Push`/`Pop` wrap `ImGui::PushFont`/`PopFont` for scoped overrides. Each UI font merges Inter (Latin) with Noto Sans Thai (`resources/fonts/inter/`, `resources/fonts/noto_sans_thai/`); the monospace slot uses Roboto Mono. Decouples font asset management from `ImGuiLayer` and gives panels a single, type-safe entry point for picking fonts.

- **2026-05-09 — Removed the ImGuizmo gizmo integration entirely.** State-corruption symptoms (handles staying active after release / phantom click latching) survived the NFD→ImGuiFileDialog swap and a four-step integration audit (move imguizmo link off the engine DLL boundary, move `BeginFrame()` inside the dockspace `Begin`, pass an explicit `ImGui::GetWindowDrawList()` to `SetDrawlist`, and add `SetGizmoSizeClipSpace(0.1f)` + `AllowAxisFlip(false)`). Decision: rip out all gizmo code (`viewport_panel::RenderGizmos`, `EditorLayer::HandleGizmoTypeChange`, the gizmo-deselect guard in `OnMouseButtonPressed`, `EditorContext::GizmoType/GizmoMode`, `TransformEditCommand`, the toolbar's gizmo combo + World/Local toggle, the `imguizmo` link in `engine/CMakeLists.txt` + `weaver/CMakeLists.txt` + the block in `vendors.cmake`) and start fresh in a future commit. **The visual transform gizmo is disabled.** Translation/rotation/scale can still be edited via the inspector input fields (those route through `PropertyEditCommand<T>` and undo correctly). The `vendor/imguizmo` submodule remains on disk but unreferenced — to be removed in a follow-up commit via `git submodule deinit -f vendor/imguizmo` + `git rm -f vendor/imguizmo`.

- **2026-05-08 — Dropped NFD (nativefiledialog-extended) in favor of ImGuiFileDialog.** Multiple sessions of trying to fix an `ImGuizmo` state-corruption bug caused by NFD stealing OS focus and blocking the main thread (symptoms: phantom mouse-down latched after dialog closed, gizmo handles becoming unresponsive). Workarounds attempted and discarded: `glfwFocusWindow` post-call, `ImGui::GetIO().ClearInputMouse()`, modified `ImGuiLayer::BlockEvents` ordering. Root cause is fundamental: NFD blocks the GLFW main thread and the OS dialog sits on top of the editor as a separate native window, so input events are dropped without ImGui ever seeing the press/release pair. Fix: replaced NFD with `ImGuiFileDialog` (vendor/imguifiledialog) — fully in-process ImGui modal, no thread blocking, no focus loss. The async API is wrapped by `Weaver::FileDialog` (`weaver/src/editor/file_dialog.{h,cpp}`): `Open / Save / PickFolder` register a callback, `Render()` polled once per frame from `EditorLayer::OnImGuiRender` dispatches the callback when the user clicks OK. Inspector callbacks capture entity by `UUID + scene shared_ptr` (not raw entity handle) so a scene swap mid-dialog is safe. `SceneManager::SaveScene/SaveSceneAs` gained an optional `on_complete` callback so the "Save Changes?" modal's pending action only fires after an async save resolves.

---

# 2. Development Roadmap

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

- [ ] **Gizmo system rewrite** *(blocking — visual transform manipulation is currently disabled)*
  - The previous ImGuizmo integration was ripped out (see Engineering Log 2026-05-09) after a multi-step audit failed to resolve persistent state corruption.
  - Rebuild: 3D translate/rotate/scale handles in the viewport, world/local mode toggle, screen-constant size, picking via screen-space proximity for axes + ray-vs-plane intersection for plane handles. Render via `ImGui::GetWindowDrawList()` (no Renderer3D dependency).
  - Reintroduce a `TransformEditCommand` (the previous one was deleted with ImGuizmo) committed on drag-end so undo batches per drag rather than per frame.
  - Until rebuilt: T/R/S editing happens only through the inspector input fields.

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

## Completed

Pre-roadmap and out-of-phase work. Roadmap items use `[x]` markers in the Phase sections above; consult `git log` for full implementation context on any entry below.

- **Editor camera serialization** — `EditorCamera` Position/Pitch/Yaw round-trip in the `.loom` file via an optional `EditorCamera*` parameter on `SceneSerializer`.
- **Asset path normalization** — all component paths (texture / Lua / audio) serialized relative to the asset dir via a `ToRelativeAssetPath()` helper.
- **Asset hot-reload** — `FileWatcher` polls disk; `Texture2D::Reload()` and `Shader::Reload()` update GPU resources in-place.
- **Audio system** — `AudioEngine` singleton (miniaudio); `AudioSourceComponent` with path / volume / loop / autoplay; YAML round-trip.
- **Prefab system** — `.lprefab` schema; "Save as Prefab" + content-browser drag-instantiate; `entity:Instantiate(path)` Lua binding.
- **Expanded Lua bindings** — `FindByTag`, `Spawn`, `Destroy`, `Instantiate`; `Physics.Raycast`; multi-arg `Log.*`.
- **Lua file watcher** — background-thread polling reloads `.lua` scripts via `OnFileChanged`.
- **Circle Collider 2D** — `CircleCollider2DComponent` using Box2D `b2Circle`; physics + serializer + inspector.
- **Entity parent-child hierarchy** — `RelationshipComponent`; world transform via `Scene::GetWorldTransform`; drag-and-drop reparenting.
- **Content browser drag & drop** — drag images onto texture slot; drag `.loom` onto viewport to open scene.
- **Sprite animation** — frame-based `AnimationComponent` cycling UV regions at configurable FPS; YAML round-trip.
- **Spritesheet helper** — auto-fills animation frames from sheet size + cell size + start row/col + frame count.
- **TextureSpecification** — per-texture `FilterMode`, `WrapMode`, `GenerateMips`; passed into `Texture2D::Create()`.
