# Loom Engine — Project Log

Dedicated tracker for project history and forward planning. AI instructions live in `CLAUDE.md`.

---

# 1. Engineering Log

A chronological ledger of multi-session debugging efforts that resulted in significant architectural pivots, and notable manual implementations worth recording for future context. New entries go at the top. Keep entries terse — one paragraph, no rehashed code.

- **2026-05-15 — Jolt Physics integration, Slice C (SphereCollider3D + Lua bindings) + editor polish.** New `SphereCollider3DComponent` (Offset vec3, Radius, Density, Friction, Restitution, IsSensor) — radius scales with `max(transform.Scale.{x,y,z})` since `JPH::SphereShape` only supports uniform scale; non-uniform entity scale picks the largest axis so the visual collider doesn't intersect geometry the artist sees enclosed. `Scene::OnPhysicsStart3D` now branches on collider type — Box takes precedence if both are present (well-defined for the unusual setup); shape, offset, friction, restitution, and isSensor are gathered into local vars before the existing offset-wrap → BodyCreationSettings flow runs unchanged. New 3D physics public API on `Scene`: `SetLinearVelocity3D / GetLinearVelocity3D / ApplyForce3D / ApplyImpulse3D` — all take/return `glm::vec3` so the public header stays Jolt-free; bodies are resolved through a static `ResolveBody3D` helper that returns `JPH::BodyID::sInvalid()` when the entity has no `Rigidbody3DComponent`, no body created yet, or the scene isn't running (caller methods early-out on invalid). `LuaEntityWrapper` exposes the four delegating methods (`SetLinearVelocity3D` etc.) that forward to `scene->...3D(handle, v)`; bound to Lua under matching names, mirroring the existing 2D physics surface. Also fixed two editor bugs: (1) `Save Changes?` and `Quit?` modals now center on the main viewport via `ImGui::SetNextWindowPos(viewport_center, Appearing, {0.5, 0.5})` so multi-monitor users don't see them appear on a non-active screen; (2) `SceneManager::SaveScene/SaveSceneAs` early-return + warn during Play mode (matches Unity's behavior — runtime mutation should never be persisted), and the File-menu items wrap in `BeginDisabled(SceneState == Play)` so they visually gray out. Validation: drop a Rigidbody3D + SphereCollider3D entity above a Static box, hit Play, sphere falls + bounces; from a Lua script, `entity:ApplyImpulse3D(Vec3(0, 5, 0))` makes a dynamic body jump.

- **2026-05-14 — Jolt Physics integration, Slice B (Rigidbody3D + BoxCollider3D, gravity sim).** New components: `Rigidbody3DComponent` (Static/Dynamic/Kinematic, FixedRotation, LinearDamping, AngularDamping; runtime body stored as raw `uint32_t RuntimeBodyID` so the header doesn't pull `<Jolt/Jolt.h>` — `0xffffffffu` matches `JPH::BodyID::cInvalidBodyID`) and `BoxCollider3DComponent` (Offset vec3, HalfExtents vec3, Density, Friction, Restitution, IsSensor). Per-scene state: `Scene` gains a forward-declared `JPH::PhysicsSystem* mPhysicsSystem3D` (pattern matches `b2WorldId mPhysicsWorld` for Box2D); init/teardown hooks `OnPhysicsStart3D` / `OnPhysicsStop3D` are private `Scene` methods called from `OnRuntimeStart` / `OnRuntimeStop` after the existing Box2D bracket. `OnPhysicsStart3D` allocates the system with 1024 max bodies/pairs/constraints and the shared layer-filter interfaces from `PhysicsEngine3D`, sets gravity to `(0, -9.81, 0)`, then walks `view<TransformComponent, Rigidbody3DComponent>` building a `JPH::BoxShape` per entity (skipping any without a `BoxCollider3DComponent`), wrapping in `RotatedTranslatedShape` only when `Offset != 0`, and creating the body via `BodyInterface::CreateAndAddBody`; static bodies use `EActivation::DontActivate`, dynamic/kinematic use `Activate`. After the batch, `OptimizeBroadPhase()` runs once. `Scene::OnUpdateRuntime` adds a 2b. step right after Box2D: `mPhysicsSystem3D->Update(ts, 1, &temp_alloc, &job_system)`, then iterates dynamic/kinematic Rigidbody3D entities and writes `BodyInterface::GetPosition/GetRotation` back to `TransformComponent` (Jolt quat -> glm::quat -> `glm::eulerAngles` for the XYZ Euler the engine stores; static bodies skipped — they don't move). `OnPhysicsStop3D` removes + destroys all bodies, then deletes the system. Helpers `ToJolt`/`FromJolt`/`EulerToJolt`/`JoltQuatToEuler`/`ToJoltMotion` live as static functions in `scene.cpp`. Density flows through `BoxShapeSettings::SetDensity` (Jolt then derives mass from shape volume); friction/restitution/damping go on the body directly. `FixedRotation` translates to `EAllowedDOFs::TranslationX|Y|Z` (locks all three rotation axes); `IsSensor` to `BodyCreationSettings::mIsSensor`. Entity handle is stashed in `BodyCreationSettings::mUserData` (uint64_t) for future collision-event resolution in Slice C. `RuntimeBodyID` is intentionally NOT serialized and is reset on copy ctor (`Rigidbody3DComponent` mirrors `AudioSourceComponent`'s pattern). Scaling: collider half-extents are multiplied by `transform.Scale` and clamped to `0.05` (Jolt's `cDefaultConvexRadius` floor) to avoid degenerate shape errors. Validation: drop a Rigidbody3D + BoxCollider3D entity above another Static one, hit Play, watch the dynamic body fall and collide.

- **2026-05-14 — Jolt Physics integration, Slice A (engine init only).** Added Jolt Physics (`vendor/jolt`) — its CMake lives in a `Build/` subdirectory so the entry is `add_subdirectory(vendor/jolt/Build Jolt)`; disabled `TARGET_HELLO_WORLD/SAMPLES/PERFORMANCE_TEST/UNIT_TESTS/VIEWER` to keep just the `Jolt` target. New `PhysicsEngine3D` singleton (`engine/include/loom/physics/physics_engine_3d.h` + `engine/src/physics/physics_engine_3d.cpp`) owns Jolt's process-wide state — registers the default allocator, factory, and types; routes `JPH::Trace` through `LOOM_CORE_TRACE` and `JPH::AssertFailed` through `LOOM_CORE_ERROR`; and constructs the shared `JPH::TempAllocatorImpl` (10MB), a `JPH::JobSystemThreadPool` (`hardware_concurrency() - 1` workers, min 1), and the three layer-filter implementations every per-scene `PhysicsSystem` will reuse. Standard two-layer split: `NON_MOVING` (static geometry) and `MOVING` (dynamic/kinematic), exposed as `PhysicsLayers3D::` `uint16_t` constants in the public header so callers don't need to pull `<Jolt/Jolt.h>` to assign a body's layer. Engine-wide includes stay light: the public header forward-declares the JPH:: types and only the .cpp pulls Jolt's heavy SIMD headers. `Init`/`Shutdown` are bracketed in `Application` between `Renderer3D` and `ScriptingEngine`. **No bodies, no components, no per-scene world yet** — that's Slice B. Validation is "boots cleanly" — log line `PhysicsEngine3D: Jolt initialized (N worker threads)` on startup and matching shutdown line on quit, no crashes.

- **2026-05-14 — Basic lighting (Blinn-Phong, scene-driven).** Replaced the placeholder hardcoded sun in `mesh.frag` with a Blinn-Phong pass that consumes scene lights. New components: `DirectionalLightComponent` (Color + Intensity; the entity's transform rotation defines the direction — light shines along local -Z, so `glm::vec4(0,0,-1,0) * world_matrix` gives the world-space propagation direction with the translation column ignored) and `PointLightComponent` (Color + Intensity + Range; position from the entity's world translation). `Renderer3D::SetLights(dir_lights, dir_count, point_lights, point_count)` is called by `Scene::OnUpdate{Editor,Runtime}` after `BeginScene` and before mesh submissions; the helper `GatherAndUploadLights` walks `view<TransformComponent, DirectionalLightComponent>` and `view<TransformComponent, PointLightComponent>`, transforms each to world space, multiplies `Color * Intensity` so the shader receives a single pre-lit color vector, and caps at the per-frame limits (`kMaxDirectionalLights = 4`, `kMaxPointLights = 16`). Light arrays upload as parallel SoA vectors via two new shader uniform helpers — `UploadUniformFloat3Array` and `UploadUniformFloatArray` — added alongside the existing `UploadUniformIntArray`. Shader uses Blinn-Phong (half-vector for specular), shininess derived from `mix(2.0, 256.0, 1.0 - roughness)` so the existing `MeshRendererComponent.Roughness` slider becomes meaningful (low roughness = tight specular highlight; high roughness = matte). Point-light attenuation is `(1 - smoothstep(0, range, dist))²` rather than physical inverse-square — predictable for game devs, no near-zero blow-up at small ranges, hard cutoff at `dist > range` skips the math entirely. A 0.2 fallback ambient runs always so scenes without any lights are still visible (avoids the "added MeshRenderer, see nothing" trap). `Metallic` plumbs through but is still a no-op until the PBR slice in Phase 5. Inspector blocks for both light types in the hierarchy panel; both round-trip through `SceneSerializer` (scene + prefab paths). Per-frame uniform uploads are folded into `Renderer3D::Submit` rather than `BeginScene` so each draw is self-contained — light state changes mid-frame would still take effect, and the cost is negligible at our scale.

- **2026-05-14 — `Renderer3D` minimum viable pass.** Stand-alone facade (`engine/include/loom/renderer/renderer_3d.h` + `engine/src/renderer/renderer_3d.cpp`) with `Init`/`Shutdown`, `BeginScene(EditorCamera)` + `BeginScene(Camera, transform)`, `EndScene`, and `Submit(mesh, albedo_color, albedo_texture, transform, roughness, metallic, entity_id)` — one draw call per submission, no batching (3D scene volumes are low and per-mesh state changes are unavoidable; will reconsider when instancing matters). Camera UBO is bound at slot 0, sharing the binding with `Renderer2D` since both write `ViewProjection` at `BeginScene` and the two passes are called in strict sequence per frame (last-write-wins is correct). Per-draw uniforms: `uModel`, `uAlbedoColor`, `uRoughness`, `uMetallic`, `uEntityID`; albedo texture binds to unit 0 with a 1×1 white fallback when null. Mesh shader (`resources/shaders/mesh.{vert,frag}`) computes the proper normal matrix `transpose(inverse(mat3(uModel)))` on the GPU and applies a single hardcoded directional light with Lambert + 0.25 ambient — this is a deliberate stopgap so meshes are visible *now*; the next slice ("Basic lighting") will replace these constants with `DirectionalLightComponent`-driven uniforms. Fragment writes both the color attachment AND the integer entity-ID attachment so 3D meshes participate in the existing mouse-pick path with no viewport-side changes. Wiring: `Application` calls `Renderer3D::Init` after `Renderer2D::Init` (and shuts down in reverse order); `Scene::OnUpdate{Editor,Runtime}` runs the 3D pass *before* the 2D pass so sprites/text/particles composite over opaque geometry — depth test was already enabled in `RenderCommand::Init`, so no other state work was needed. Lazy load mirrors the texture pattern: `DrawMeshEntity` resolves `MeshPath` and `AlbedoTexturePath` against `Project::GetAssetFileSystemPath` and re-fetches via `AssetManager` when the absolute path changes. `Roughness` and `Metallic` plumb through to shader uniforms but visually do nothing until the PBR slice in Phase 5.

- **2026-05-14 — `MeshRendererComponent` (mesh + material in one component).** Rejected the roadmap's 3-component split (`MeshComponent` + `MaterialComponent` + `MeshRendererComponent`-as-marker) in favor of a single combined component, mirroring `SpriteRendererComponent`'s pattern — one Add Component click, one inspector block. The marker-component split was hypothetical (zero data, no current need for cross-entity material sharing). Component carries `MeshPath` (project-relative) + cached `std::shared_ptr<MeshAsset>` runtime handle, `AlbedoColor`, `AlbedoTexturePath` + cached `std::shared_ptr<Texture2D>` handle, plus `Roughness` and `Metallic` sliders. Path strings are kept in sync with the live asset's `GetPath()` and serialized via the existing `ToRelativeAssetPath` helper. Wired through scene cloning (`CopyComponent<MeshRendererComponent>` in `scene.cpp`), scene serializer (write block + scene-deserialize block + prefab-deserialize block in `scene_serializer.cpp`), and the inspector (`weaver/src/panels/scene_hierarchy_panel.cpp`) — mesh slot accepts content-browser drag-drop for `.glb`/`.gltf` and a FileDialog browse fallback; albedo texture slot follows the existing sprite-texture drop pattern (`.png`/`.jpg`/`.jpeg`/`.bmp`/`.tga`). Inspector renders vertex/index counts under the mesh button as a load confirmation. **Nothing renders yet** — `Renderer3D` is the next roadmap item; this slice is purely data + UI so the Game Developer can author scene content while the renderer is in flight.

- **2026-05-14 — Mesh loading via cgltf (Phase 4 kickoff, asset slice).** Picked **cgltf** over assimp for GLTF/GLB import — single-header C, zero deps, header-only INTERFACE target in `cmake/vendors.cmake` (`CGLTF_IMPLEMENTATION` defined exactly once in `mesh_asset.cpp`). Trade-off accepted: no native FBX/OBJ ingest, but GLTF is the modern interchange standard and Blender/Maya/Substance all export it; sidesteps assimp's 200K-LOC build cost. New `Loom::MeshAsset` (`engine/include/loom/renderer/mesh_asset.h` + `engine/src/renderer/mesh_asset.cpp`) owns a single `VertexArray` built from `MeshVertex { vec3 Position; vec3 Normal; vec2 TexCoord; }`. `MeshAsset::Create(path)` walks `data->meshes[*].primitives[*]`, concatenates all triangle primitives into one VBO/IBO (positions required; missing normals → `(0,0,1)`, missing UVs → `(0,0)`; non-indexed primitives get a sequential index list); skips non-triangle primitives with a warning. Multi-mesh GLBs collapse to a single VAO for now — submesh / per-material splits will come with the `MeshComponent` + `MaterialComponent` slice. Added a data-bearing `VertexBuffer::Create(const void* data, uint32_t size)` overload (and OpenGL ctor) so static mesh uploads don't need the dynamic-batch `nullptr`-then-`SetData` two-step. `AssetManager::GetMesh(path)` mirrors the texture/shader cache pattern (`weak_ptr` map, mutex, `Trim`/`Clear` hooks); no hot-reload watcher hookup yet. No `MeshComponent` and no `Renderer3D` — those are the next two roadmap items, intentionally separated so this slice can be validated in isolation by calling `AssetManager::GetMesh("path/to/x.glb")` and checking the trace log for vertex/index counts.

- **2026-05-14 — Custom gizmo system, ImGuizmo-free.** Built a from-scratch transform gizmo that lives entirely in `weaver/src/panels/viewport_panel.{h,cpp}` and renders through `ImGui::GetWindowDrawList()` — no Renderer3D dependency, no vendor library. Supports Translate (3 axes + 3 plane handles), Rotate (3 rings) and Scale (3 axes + uniform center). Picking is screen-space: `Loom::Math::DistancePointToSegment2D` for axis/ring proximity and `PointInTriangle2D` for plane quads (8px / 6px thresholds). Dragging uses `Loom::Math::ScreenToRay` + `RayPlaneIntersect`: for each handle we pick the plane (axis-aligned camera-facing for axis handles, the plane itself for plane handles, perpendicular-to-axis for rotation rings) and intersect the mouse ray with it; `ClosestPointOnLine` snaps the hit back to the axis for line-constrained motion. Screen-constant handle length is derived from the perspective projection: `world_per_pixel = 2 * d * (1/proj[1][1]) / viewport_height`, no FOV accessor needed. Parent-aware: world deltas are converted back to local space via `inverse(parent_world)` before being written to `TransformComponent`, and rotation composes as `T_pivot · R(axis,θ) · T_pivot⁻¹ · world_start` then decomposes through `Math::DecomposeTransform`. New `Math::` helpers landed: `Ray`, `ScreenToRay`, `WorldToScreen`, `RayPlaneIntersect`, `ClosestPointOnLine`, `DistancePointToSegment2D`. `TransformEditCommand` was reintroduced (full `TransformComponent` before/after snapshot — one command type for T/R/S). Drag-start snapshots the local transform + parent world; drag-end pushes the command only if any component changed beyond `1e-5f`. `EditorContext` gained `GizmoOp` (None/Translate/Rotate/Scale) and `GizmoMode` (World/Local). Shortcuts: W/E/R/Q switch operation, X toggles space — gated on `ViewportFocused && !WantTextInput`. Toolbar gained functional T/R/S/- buttons and a Local/World toggle (visual styling deferred to UX pass). `EditorLayer::OnMouseButtonPressed` now calls `mViewportPanel.BeginGizmoDragIfHovered()` before falling back to entity selection so the gizmo claims clicks first.

- **2026-05-14 — CPU particle system added.** New `ParticleComponent` (engine/include/loom/scene/components.h) carries emitter config (Point/Box/Circle shape, world-vs-local sim space, spawn rate, lifetime/velocity/gravity ranges, color/size over normalized lifetime, max-particle cap, optional texture) plus a per-emitter `Live` pool and `SpawnAccumulator` of runtime state (not serialized). All sim + render logic lives as inline static helpers in `scene.cpp` (`TickParticles`, `DrawParticles`, `UpdateAndDrawParticleEntity`) wired into both `OnUpdateEditor` and `OnUpdateRuntime` — particles tick in the editor too so the Game Developer gets a live FX preview without entering Play. Rendering reuses the existing `Renderer2D::DrawQuad` matrix overloads (one quad per particle, batched by the existing renderer); no new entry point. World-space mode keeps particles flat at the emitter's Z and ignores emitter rotation/scale; local-space mode multiplies by the full emitter world matrix so particles inherit movement. `Live` is cleared on `OnRuntimeStart` and `OnRuntimeStop` so editor preview state never leaks across mode transitions. Inspector UI is left to a follow-up commit per division of labor.

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

- [x] **Gizmo system rewrite**
  - Custom 3D Translate/Rotate/Scale handles rendered via `ImGui::GetWindowDrawList()` — no Renderer3D, no vendor library.
  - World/Local mode toggle; screen-constant size; picking via screen-space proximity for axes/rings and point-in-triangle for plane quads; dragging via ray-vs-plane intersection.
  - `TransformEditCommand` reintroduced — drag-start snapshots the local transform; drag-end pushes one command so undo batches per drag rather than per frame.
  - Shortcuts: W/E/R/Q switch operation, X toggles space.
  - **Future:** revisit migrating to ImGuizmo once the input pipeline is fully validated. The original 2026-05-09 removal was driven by state-corruption symptoms that may have stemmed from NFD's focus-stealing (since replaced by `ImGuiFileDialog`); the root cause was never proven. ImGuizmo would shrink the gizmo code surface and inherit upstream improvements; the trade-off is reintroducing a vendor dep + bridging its per-frame matrix delta into our `TransformEditCommand` batching. Not a priority — current implementation works.

- [x] **Particle system**
  - `ParticleComponent`: Point/Box/Circle emitter shape, world-vs-local simulation space, spawn rate, lifetime range, velocity range, gravity + scale, rotation speed, color/size over normalized lifetime, max-particle cap, optional texture
  - CPU-simulated inside `Scene::OnUpdate{Editor,Runtime}` (live preview in the editor); rendered via batched `Renderer2D::DrawQuad`
  - YAML round-trip in scene + prefab paths; runtime pool cleared on `OnRuntimeStart`/`OnRuntimeStop`
  - Inspector UI is the Game Developer's responsibility (per current division of labor)

---

## Phase 4 — 3D Foundation

*Correct dependency order: assets first, renderer second, lighting third.*

- [x] **Mesh loading**
  - Added **cgltf** submodule (`vendor/cgltf`, single-header C, INTERFACE target in `cmake/vendors.cmake`)
  - `MeshAsset` (`engine/include/loom/renderer/mesh_asset.h`): VAO/VBO/IBO with `MeshVertex { vec3 Position; vec3 Normal; vec2 TexCoord; }`; concatenates all triangle primitives in a GLTF/GLB into one mesh
  - `AssetManager::GetMesh(path)`: weak_ptr cache mirroring the texture/shader pattern
  - Required a data-bearing `VertexBuffer::Create(const void* data, uint32_t size)` overload for static mesh uploads

- [x] **Mesh & material components** *(scope-reduced — single component instead of 3)*
  - `MeshRendererComponent`: combined mesh + material data in one component, matching `SpriteRendererComponent`'s pattern (single Add Component click, single inspector block)
  - Fields: `MeshPath` (project-relative) + cached `Mesh`, `AlbedoColor`, `AlbedoTexturePath` + cached `AlbedoTexture`, `Roughness`, `Metallic`
  - Inspector: mesh slot with content-browser drag-drop (`.glb`/`.gltf`) + FileDialog browse, vertex/index count readout, albedo color picker, albedo texture slot, roughness/metallic sliders
  - Scene + prefab YAML round-trip; runtime caches resolved via `Project::GetAssetFileSystemPath` on load
  - **Note:** the original 3-component split (`MeshComponent` + `MaterialComponent` + `MeshRendererComponent`) was rejected as over-engineered; the marker `MeshRendererComponent` would have added zero data, and material sharing across entities was a hypothetical we don't need yet. If we ever need that, splitting later is straightforward.

- [x] **Renderer3D**
  - `Renderer3D::Submit(mesh, albedo_color, albedo_texture, transform, roughness, metallic, entity_id)` — one draw call per submission (no batching)
  - `BeginScene(EditorCamera)` and `BeginScene(Camera, transform)` mirror Renderer2D; both write the shared UBO at binding 0
  - Pipeline isolated from Renderer2D — own `mesh.{vert,frag}` (single hardcoded directional Lambert + ambient as a stopgap until the lighting slice replaces it with `DirectionalLightComponent`-driven shading); fragment writes the entity-ID picking attachment so the existing mouse-pick path catches 3D meshes
  - `Scene::OnUpdate{Editor,Runtime}` runs the 3D pass before the 2D pass so sprites overlay opaque geometry (depth test was already enabled at `RenderCommand::Init`)

- [x] **Basic lighting**
  - `DirectionalLightComponent` (Color, Intensity; direction taken from entity rotation, local -Z)
  - `PointLightComponent` (Color, Intensity, Range; position taken from entity translation)
  - Blinn-Phong fragment pass replaces the previous hardcoded sun. Up to 4 directional + 16 point lights per scene; shininess derived from `MeshRendererComponent.Roughness`. Always-on fallback ambient (0.2) keeps unlit scenes visible.
  - `Renderer3D::SetLights(...)` API; `Scene::OnUpdate{Editor,Runtime}` gathers and uploads light state per frame before mesh submissions.

- [x] **3D physics**
  - Added **Jolt Physics** submodule (`vendor/jolt`)
  - `PhysicsEngine3D` singleton owns process-wide Jolt state alongside Box2D
  - `Rigidbody3DComponent`, `BoxCollider3DComponent`, `SphereCollider3DComponent`
  - `Scene::OnPhysicsStart3D` / `OnPhysicsStop3D` lifecycle, gravity sim, body→transform sync
  - Lua bindings: `Entity:SetLinearVelocity3D / GetLinearVelocity3D / ApplyForce3D / ApplyImpulse3D`

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
