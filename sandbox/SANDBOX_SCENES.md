# Sandbox Scene Plan

Authoring queue + **build guide** for the sandbox project's demo scenes. Each
scene dogfoods a slice of the engine end-to-end; together they are the
validation gate before Phase 6. Rewritten 2026-05-21 to match the current
engine (PBR + IBL, CSM shadows, scene-owned environment, tilemap per-tile
collision, 3D physics, prefabs), to design the scenes as **living showcases**
(each one carries explicit extension points so future-roadmap features slot
into the *same* scene instead of needing a rewrite), and to give the Game
Developer a concrete step-by-step recipe for every scene.

**Naming:** scenes use generic, descriptive names — no numeric prefix, no
implied tour order. Each scene is a self-contained feature demo.

**Workflow:** the Game Developer builds each scene in Weaver from the build
steps below; the AI writes the Lua scripts the scene needs. After a scene saves
and validates, tick its checkbox and move to the next.

---

## 1. Asset & file organization

Assets are organized **per scene**, with one shared folder for assets that
genuinely span multiple scenes (primitive meshes, HDR environments, fonts).
Duplicating a 200 MB HDR or `sphere.glb` into six folders would be wasteful and
make "fix the asset once" impossible — hence the `common/` exception.

```
sandbox/
  sandbox.loomproj
  assets/
    common/                     # cross-scene assets — referenced by many scenes
      models/                   #   cube.glb, sphere.glb, capsule.glb, plane.glb
      environments/             #   *.hdr equirectangular skyboxes
      fonts/                    #   *.ttf
    scenes/                     # all *.loom scene files live here, flat
      platformer_2d.loom
      material_gallery.loom
      physics_playground.loom
      character_controller.loom
      next_area.loom
      particle_lab.loom
    platformer_2d/              # one folder per scene — scene-specific assets
      textures/
      sounds/
      scripts/
      prefabs/
    material_gallery/
      ...                       # only the subfolders that scene actually needs
    physics_playground/
    character_controller/
    particle_lab/
```

**Path rules (important):**

- Every path stored on a component (texture, mesh, font, script, audio) is
  **relative to `assets/`**. A sprite at
  `assets/platformer_2d/textures/player_sheet.png` is stored as
  `platformer_2d/textures/player_sheet.png`.
- `Scene.Load(...)` in Lua takes the same asset-relative path, e.g.
  `Scene.Load("scenes/next_area.loom")`.
- Create only the subfolders a scene needs — a script-free visual scene needs
  no `scripts/` folder.

---

## 2. Weaver authoring primer (read once)

Common UI actions referenced by every scene below, so the per-scene steps stay
short. Exact menu labels may differ slightly — the *flow* is what matters.

| Action | How |
| --- | --- |
| **New scene** | `File → New Scene`, then `File → Save Scene As…` → save into `assets/scenes/` with the scene's name |
| **Create entity** | Right-click empty space in the **Scene Hierarchy** → *Create Entity*. It spawns with Tag + Transform already attached |
| **Duplicate entity** | Select an entity → **Ctrl+D**, or right-click it → *Duplicate Entity*. Deep-copies all components + child subtree; the copy is selected and undoable |
| **Rename / set Tag** | Select the entity; edit the **Tag** field at the top of the Inspector. Scripts find entities by Tag, so spelling matters |
| **Add a component** | Select the entity → **Add Component** button in the Inspector → pick from the list |
| **Parent an entity** | Drag one entity onto another in the Scene Hierarchy |
| **Set the active camera** | The camera entity's `CameraComponent` must have **Primary = true**. Only one Primary camera per scene |
| **Save as Prefab** | Right-click a fully-configured entity in the Scene Hierarchy → *Save as Prefab* → choose the scene's `prefabs/` folder. Produces a `.lprefab` |
| **Instantiate a Prefab** | Drag the `.lprefab` from the **Content Browser** into the viewport (or hierarchy) |
| **Skybox / IBL** | `View → Scene Properties` → set the **Skybox HDR** path. Drives skybox + diffuse irradiance + specular reflections |
| **Collider debug overlay** | Toolbar **Settings** popup → enable the colliders overlay. Works in Edit *and* Play |
| **Play / Stop** | Toolbar play/stop buttons. Play runs scripts + physics; Stop reverts to the pre-Play scene state |
| **Save** | `Ctrl+S`. Note: Save is disabled during Play — Stop first |

**Coordinate conventions:**

- `Transform.Rotation` is shown in **degrees** in the Inspector (stored as
  radians internally — you never see the radians).
- 2D scenes work in the XY plane; Z is depth/layering. 3D scenes use full XYZ.
- A `DirectionalLight` shines along its entity's **local −Z** after rotation —
  rotate the entity to aim the light; position is irrelevant.
- Box colliders use **half-extents** (Box2D `Size`, Jolt `HalfExtents`): a
  1×1 world-unit box has half-extents `(0.5, 0.5)`.

---

## 3. Roadmap coverage

Which scene exercises which engine feature — so coverage gaps stay visible and,
when a roadmap feature ships, it's obvious which scene to extend.

| Feature | Status | Scene(s) |
| --- | --- | --- |
| 2D sprites + sprite animation | shipped | `platformer_2d` |
| Tilemap + per-tile collision | shipped | `platformer_2d` |
| 2D physics (Box2D, sensors, events) | shipped | `platformer_2d` |
| Audio (attached + tag-resolved SFX) | shipped | `platformer_2d` |
| Text / HUD | shipped | `platformer_2d`, `next_area` |
| Lua scripting | shipped | `platformer_2d`, `physics_playground`, `character_controller`, `next_area` |
| **Prefabs** (`.lprefab`, drag-instantiate, `Instantiate`) | shipped | `platformer_2d` (coins), `character_controller` (pickups) |
| Mesh loading (glTF/GLB) | shipped | `material_gallery`, `physics_playground`, `character_controller`, `next_area` |
| PBR materials (factor-based) | shipped | `material_gallery`, `physics_playground`, `character_controller` |
| IBL / HDR skybox (Scene Properties) | shipped | `material_gallery`, `physics_playground` |
| CSM shadows | shipped | `material_gallery`, `physics_playground`, `character_controller` |
| 3D physics (Jolt — Box/Sphere/Capsule) | shipped | `physics_playground`, `character_controller` |
| Scene transitions | shipped | `character_controller` ↔ `next_area` |
| Particle system | shipped | `particle_lab` |
| Material texture maps (metalRough/normal/AO/emissive) | Phase 5 follow-up | **`material_gallery`** — Hero pedestal *(extend)* |
| Post-processing / bloom | Phase 7 | **`material_gallery`** — *(extend)* |
| 3D skeletal animation | Phase 8 | **`skeletal_character`** *(planned)*, `character_controller` Player *(mesh swap)* |

---

## Scene — `platformer_2d.loom`

**Validates:** 2D sprites + sprite animation + tilemap with **per-tile
collision** + 2D physics (Rigidbody2D, BoxCollider2D, sensor pickups) +
collision events + audio (player-attached + tag-resolved SFXPlayer pattern) +
text + Lua scripting + **prefabs** — the whole 2D stack in one scene.

**Theme:** mini platformer — player jumps between platforms collecting spinning
coins, score in the HUD, ambient music.

### Required assets (placed — swamp tileset pack)

All under `assets/platformer_2d/`, except the shared font. Sourced mostly from
the *free-swamp-game-tileset-pixel-art* pack.

| Asset | Path | Layout |
| --- | --- | --- |
| Player sprite sheet (cat) | `platformer_2d/textures/player_sheet.png` | 962×330 — **12 cols × 5 rows**, ~80×66 px per cell. Row 0 = idle/stand, row 1 = jump/leap, row 4 = walk cycle |
| Tileset | `platformer_2d/textures/tileset.png` | 320×192 swamp tileset — floor + platform tiles |
| Coin sprite sheet | `platformer_2d/textures/coin.png` | 40×10 — **4 frames, 10×10 px each** (spin) |
| Background | `platformer_2d/textures/background.png` | 576×324 swamp-forest backdrop |
| Font | `common/fonts/inter_regular.ttf` | shared |
| Music | `platformer_2d/sounds/music.wav` | ambient loop |
| Jump SFX | `platformer_2d/sounds/jump.wav` | short |
| Pickup SFX | `platformer_2d/sounds/pickup.wav` | short |

There is no separate platform sprite — **mid-air platforms are painted into the
Level tilemap** alongside the floor (one tilemap holds all level geometry; its
per-tile collider covers both).

### Build steps

Build in batches; the AI walks you through each. Entities: MainCamera,
Background, Level, Player, SFXPlayer, Coin ×3, Score, Music.

1. **New scene** → save as `assets/scenes/platformer_2d.loom`.
2. **MainCamera** — Create Entity, Tag `MainCamera`. Add `CameraComponent`:
   Projection = **Orthographic**, Size ≈ `12`, **Primary = true**.
   Transform position `(0, 2, 10)`.
3. **Background** — Create Entity, Tag `Background`. Add `SpriteRenderer`,
   Texture = `platformer_2d/textures/background.png`. Transform position
   `(0, 2, -1)` (behind everything), Scale `(24, 14, 1)` — adjust to frame.
4. **Level** (tilemap — floor + platforms) — Create Entity, Tag `Level`. Add
   `TilemapComponent`:
   - Spritesheet = `platformer_2d/textures/tileset.png`.
   - `Columns ≈ 24`, `Rows ≈ 14`, `TileWidth = TileHeight = 1`.
   - `SheetColumns` / `SheetRows` = the tileset's actual tile grid (set so the
     palette divides cleanly onto the tiles).
   - Transform position so the map is centered on the camera view.
   - Press **B** (or the Inspector's *Paint in Viewport* toggle): paint a solid
     floor along the bottom rows, then paint a few raised platform clusters
     mid-air for the player to jump between.
   - In the tile palette, **Shift+click** every tile used for floor/platforms
     to mark it **Solid** (turns red). The tilemap auto-generates one merged
     static collider on Play — no separate collider entities needed.
5. **Player** — Create Entity, Tag `Player`. Transform position above the
   floor, e.g. `(-6, 4, 0)`, **Scale `(3, 3, 1)`**. The cat art is 32×32 px
   inside an 80×64 px canvas cell, so at Scale 1 the visible cat is only
   ~0.4×0.5 units — Scale 3 brings it to ~1.2×1.5 units. Add:
   - `SpriteRenderer` — Texture = `platformer_2d/textures/player_sheet.png`.
   - `AnimationComponent` — set `PickerCellWidth = 80`, `PickerCellHeight = 66`.
     Add three clips named exactly **`idle`**, **`run`**, **`jump`**. With the
     Spritesheet Picker: `idle` ← a few frames from **row 0**; `run` ← the
     **row 4** walk cycle; `jump` ← the 3 frames from **row 1** (launch / peak
     / fall, in that order). FrameDuration ≈ `0.1`; `idle`/`run` Loop = on,
     `jump` Loop = off (the script drives its frames by velocity).
   - `Rigidbody2D` — Type = **Dynamic**, **FixedRotation = true**.
   - `BoxCollider2D` — Size `(0.2, 0.24)`. The runtime collider is
     `Size × Transform.Scale`, so this gives a world collider of ~0.6×0.72
     that hugs the cat. If you retune it, set `PlayerHalfHeight` in
     `player.lua` to the collider's **world** Y half-extent (`Size.y × Scale.y`).
   - `LuaScript` — ScriptPath = `platformer_2d/scripts/player.lua`.
   - `AudioSource` — AssetPath = `platformer_2d/sounds/jump.wav`,
     AutoPlay = false, Volume = 0.6.
6. **SFXPlayer** — Create Entity, Tag `SFXPlayer`. Add `AudioSource`:
   AssetPath = `platformer_2d/sounds/pickup.wav`, AutoPlay = false,
   Volume = 0.7. (Coins resolve this by Tag and trigger it — a coin destroys
   itself, so it can't own the SFX.)
7. **Coin_1** — Create Entity, Tag `Coin` (exact tag — the player ground-ray
   filters it). Transform **Scale `(0.4, 0.4, 1)`** (a coin frame is 10×10 px
   → 1 unit at Scale 1, far too big; Scale 0.4 makes it ~0.4 units). Position
   over a platform. Add:
   - `SpriteRenderer` — Texture = `platformer_2d/textures/coin.png`.
   - `AnimationComponent` — `PickerCellWidth = PickerCellHeight = 10`. One clip
     named `spin`, all 4 frames, FrameDuration ≈ `0.12`, Loop = on. Set
     `CurrentClip = spin` so it auto-plays.
   - `Rigidbody2D` — Type = **Static**.
   - `BoxCollider2D` — Size `(0.5, 0.5)`, **IsSensor = true** (world sensor
     `Size × Scale` ≈ 0.2×0.2 — snug to the coin).
   - `LuaScript` — ScriptPath = `platformer_2d/scripts/collectible.lua`.
8. **Coin prefab** — with Coin_1 fully configured, right-click it →
   *Save as Prefab* → `platformer_2d/prefabs/coin.lprefab`. Drag the prefab
   from the Content Browser into the scene twice (**Coin_2**, **Coin_3**),
   positioning each over a different platform. Dogfoods the prefab pipeline.
9. **Score** (HUD) — Create Entity, Tag `Score`. Add `TextComponent`:
   FontPath = `common/fonts/inter_regular.ttf`, Text = `Score: 0`,
   FontSize ≈ 0.6, Color white. Position in a top corner of the camera view.
10. **Music** — Create Entity, Tag `Music`. Add `AudioSource`:
    AssetPath = `platformer_2d/sounds/music.wav`, AutoPlay = true, Loop = true,
    Volume = 0.3.
11. **Save** (`Ctrl+S`).

### Scripts (written — `platformer_2d/scripts/`)

- `player.lua` — A/D lateral movement (MoveSpeed 5), rising-edge-detected jump
  (velocity-based, JumpSpeed 9) gated by a downward ground ray; switches the
  `idle` / `run` / `jump` clips; flips the sprite to face the move direction;
  triggers the attached jump `AudioSource`. Tunables are locals at the top.
- `collectible.lua` — `OnSensorBegin`; ignores non-`Player`; bumps the `Score`
  HUD (parses `Score: N` via `GetText`/`SetText`), triggers `SFXPlayer` by Tag,
  then `entity:Destroy()`.

### Validation checklist

- [ ] Player falls and lands on the tilemap floor (tilemap per-tile collision)
- [ ] A/D moves laterally; Space jumps only when grounded (held Space doesn't auto-rejump)
- [ ] Player animates: `idle` when still, `run` when moving, `jump` when airborne; sprite flips to face the move direction
- [ ] Player lands and stands on the painted mid-air platforms
- [ ] Walking into a coin bumps `Score: N → N+1` and the coin disappears
- [ ] Coin spin animation loops smoothly
- [ ] Pickup SFX plays after the coin disappears (SFXPlayer-by-Tag pattern)
- [ ] Jump SFX plays on each jump
- [ ] Background music auto-plays from start
- [ ] All three coins instantiated from `coin.lprefab`

---

## Scene — `material_gallery.loom`

**Validates:** the whole Phase 5 rendering pipeline — metallic-roughness PBR,
image-based lighting from a scene-owned HDR environment, and cascaded shadow
maps. Pure visual; no scripts.

**Theme:** a material showcase — a grid of spheres sweeping Roughness and
Metallic under an HDR sky, all casting shadows onto a ground plane.

> **Extension points (this is the rendering test bed):**
> - **Hero pedestal** — a reserved display-stand slot, set **off to one side**
>   so it never occludes the sphere grid. *Today:* a plain PBR sphere.
>   *Phase 5 follow-up:* swap to a fully-textured model (DamagedHelmet) once
>   metalRoughness / normal / AO / emissive maps land. *Phase 7:* an emissive
>   object beside it becomes the bloom reference.
> - Keep the sphere grid as labelled rows so a **normal-mapped row** can be
>   appended later without disturbing the existing sweep.

### Required assets

| Asset | Path | Notes |
| --- | --- | --- |
| Sphere mesh | `common/models/sphere.glb` | Smooth-shaded UV sphere (shared with later scenes) |
| Plane mesh | `common/models/plane.glb` | Ground plane; a flattened cube works too |
| HDR environment | `common/environments/<name>.hdr` | Equirectangular HDR (shared with `physics_playground`) |

Source suggestions: [Poly Haven](https://polyhaven.com/hdris) (HDRIs),
[Khronos glTF Sample Models](https://github.com/KhronosGroup/glTF-Sample-Assets)
(meshes).

### Build steps

1. **New scene** → save as `assets/scenes/material_gallery.loom`.
2. **Skybox / IBL** — `View → Scene Properties` → set **Skybox HDR** to
   `common/environments/<name>.hdr`. This drives the skybox, diffuse
   irradiance, and specular reflections for every PBR surface.
3. **MainCamera** — Create Entity, Tag `MainCamera`. Add `CameraComponent`:
   Projection = **Perspective**, **Primary = true**. Transform position
   `(0, 1.5, 9)`, Rotation X ≈ `-8°` (tilt slightly down).
4. **Sun** — Create Entity, Tag `Sun`. Add `DirectionalLightComponent`,
   Intensity ≈ `2.0`. Rotate the entity so its local −Z aims down at the grid
   (start X ≈ `-50°`, Y ≈ `-30°`; verify by the shadows in the viewport).
5. **Ground** — Create Entity, Tag `Ground`. Add `MeshRendererComponent`:
   Mesh = `common/models/plane.glb`, **AlbedoColor a dark neutral**
   `(0.20, 0.20, 0.23, 1.0)`, Roughness ≈ `0.9`, Metallic = `0`. A white
   ground washes out the white dielectric spheres and hides their shadows —
   a dark ground makes both read. Transform position `(0, -1.5, 0)`,
   Scale `(20, 1, 20)`.
6. **Sphere grid** — Create 10 entities (`Sphere_R0C0` … `Sphere_R1C4`), each
   with `MeshRendererComponent` Mesh = `common/models/sphere.glb`. Lay them out
   as a **5 × 2 grid**, spacing ≈ 2.5 units, centered above the ground:
   - **Columns sweep Roughness:** `0.0 / 0.25 / 0.5 / 0.75 / 1.0`.
   - **Bottom row:** Metallic = `0` (dielectric).
   - **Top row:** Metallic = `1` (metal) — give it a tinted AlbedoColor
     (e.g. gold `(1.0, 0.78, 0.34, 1.0)`) so the metallic tint reads clearly.
7. **HeroPlinth** — Create Entity, Tag `HeroPlinth`. Add
   `MeshRendererComponent` Mesh = `common/models/cube.glb` (from `common/`),
   **AlbedoColor a dark neutral** `(0.15, 0.15, 0.17, 1.0)`, Roughness ≈ `0.6`,
   Metallic = `0`. Scale it into a short wide plinth. Position it **off to one
   side, clear of the camera→grid sightline** (e.g. front-left, past the
   leftmost column) so it never occludes the sphere grid — it is a separate
   display stand, not a centerpiece.
8. **Hero** — Create Entity, Tag `Hero`. Add `MeshRendererComponent`
   Mesh = `common/models/sphere.glb`, mid-roughness dielectric. Position it
   on top of the plinth. **This is the reserved extension slot** (see note).
9. **Save** (`Ctrl+S`).

### Validation checklist

- [ ] Skybox renders behind the spheres (set via Scene Properties)
- [ ] Low-roughness spheres show sharp environment reflections; high-roughness spheres look matte — a clean visual sweep across each row
- [ ] Metal row reflects the environment and tints by AlbedoColor; dielectric row keeps a diffuse look with a tighter specular highlight
- [ ] Every sphere casts a shadow onto the ground plane (CSM)
- [ ] Enter Play — with a Skybox HDR assigned, the runtime looks identical to edit mode; clear the Skybox path and Play goes dark (no fallback in runtime, by design)

---

## Scene — `physics_playground.loom`

**Validates:** mesh loading (GLTF/GLB) + Renderer3D PBR + Jolt 3D physics with
all three collider types (Box / Sphere / Capsule) + 3D collision events +
collider debug rendering.

**Theme:** physics playground — dynamic cubes, spheres, and capsules raining
onto a tilted floor; a collision logger script prints contact events.

### Required assets

| Asset | Path | Notes |
| --- | --- | --- |
| Cube mesh | `common/models/cube.glb` | Primitive (shared) |
| Sphere mesh | `common/models/sphere.glb` | Primitive (shared) |
| Capsule mesh | `common/models/capsule.glb` | Primitive (shared) |
| HDR environment | `common/environments/<name>.hdr` | Reuses `material_gallery`'s HDR |
| *Floor albedo* (optional) | `physics_playground/textures/wood.png` | Factors-only is also fine |

### Build steps

1. **New scene** → save as `assets/scenes/physics_playground.loom`.
2. **Skybox / IBL** — `View → Scene Properties` → set **Skybox HDR** to the
   shared `common/environments/<name>.hdr` so the props are lit.
3. **MainCamera** — Tag `MainCamera`, `CameraComponent` Perspective,
   **Primary = true**. Transform position `(0, 6, 14)`, Rotation X ≈ `-20°`.
4. **Sun** — Tag `Sun`, `DirectionalLightComponent` Intensity ≈ `2.0`, rotated
   to shine down (as in `material_gallery`).
5. **Floor** — Tag `Floor`. Add `MeshRendererComponent` (Mesh =
   `common/models/cube.glb`), `Rigidbody3D` (Type = **Static**),
   `BoxCollider3D`. Transform Scale `(12, 0.5, 12)`, Rotation Z ≈ `10°` so
   props slide. Set `BoxCollider3D.HalfExtents` to `(6, 0.25, 6)` (the scaled
   cube's half-size).
6. **Falling props** — create ~3–4 of each primitive:
   - **Cube_*** — Tag `Cube`. `MeshRendererComponent` (cube.glb),
     `Rigidbody3D` (Dynamic), `BoxCollider3D`, `LuaScript` =
     `physics_playground/scripts/collision_logger.lua`.
   - **Sphere_*** — Tag `Sphere`. `MeshRendererComponent` (sphere.glb),
     `Rigidbody3D` (Dynamic), `SphereCollider3D` (Radius matched to the mesh),
     same `LuaScript`.
   - **Capsule_*** — Tag `Capsule`. `MeshRendererComponent` (capsule.glb),
     `Rigidbody3D` (Dynamic), `CapsuleCollider3D` (Radius + HalfHeight matched
     to the mesh), same `LuaScript`.
   Position each prop at staggered heights `(x, 6…10, z)` and offset X/Z so
   they collide on the way down.
7. **Save** (`Ctrl+S`).

### Scripts (AI to write)

- `physics_playground/scripts/collision_logger.lua` — `OnCollisionBegin(other)`
  / `OnCollisionEnd(other)` log the other entity's Tag via `Log.Info`.

### Validation checklist

- [ ] All props fall under gravity and rest on / slide down the tilted floor
- [ ] Box, Sphere, and Capsule colliders all behave correctly (no interpenetration; capsules roll/tip plausibly)
- [ ] Console + `loom.log` show collision-begin/end lines as props settle
- [ ] Collider debug overlay (toolbar Settings) draws each shape correctly
- [ ] PBR meshes are lit by the HDR environment

---

## Scene — `character_controller.loom`

**Validates:** capsule rigidbody character controller + 3D Lua physics API
(`SetLinearVelocity3D`, `ApplyImpulse3D`) + sensor pickups in 3D + camera
follow + scene transition (`Scene.Load`) + **prefabs**.

**Theme:** a capsule character walks around a small arena, collects sensor
pickups, and steps into a "level exit" zone that loads `next_area`.

> **Extension point:** the **Player** entity is the reserved slot for Phase 8
> 3D skeletal animation — when skinned meshes land, swap the capsule mesh for a
> rigged character with walk / idle / jump clips; the controller script and
> physics body stay as-is, no scene restructuring.

### Required assets

Reuses `common/models/` primitives and `material_gallery`'s HDR — no
scene-specific 3D assets needed. Pickups are small scaled cubes/spheres.

### Build steps

1. **New scene** → save as `assets/scenes/character_controller.loom`.
2. **Skybox / IBL** — `View → Scene Properties` → set **Skybox HDR** to the
   shared HDR.
3. **MainCamera** — Tag `MainCamera`, `CameraComponent` Perspective,
   **Primary = true**. Place it behind/above where the player starts;
   `character.lua` drives the follow each frame, so the exact start pose only
   needs to be roughly behind the player.
4. **Sun** — Tag `Sun`, `DirectionalLightComponent`, rotated to shine down.
5. **Floor** — Tag `Floor`. `MeshRendererComponent` (cube.glb),
   `Rigidbody3D` (Static), `BoxCollider3D`. Scale into a wide flat arena
   (e.g. `(20, 0.5, 20)`); set `HalfExtents` to the scaled half-size.
6. **Walls** — create ~4 wall entities ringing the arena. Each:
   `MeshRendererComponent` (cube.glb), `Rigidbody3D` (Static),
   `BoxCollider3D` matched to the scaled wall.
7. **Player** — Tag `Player`. Add `MeshRendererComponent` (capsule.glb),
   `Rigidbody3D` (Dynamic, **FixedRotation = true**), `CapsuleCollider3D`
   (Radius + HalfHeight matched to the mesh), `LuaScript` =
   `character_controller/scripts/character.lua`. Position above the floor.
8. **Pickup prefab** — create one **Pickup** entity: Tag `Pickup`,
   `MeshRendererComponent` (a small scaled cube or sphere), `Rigidbody3D`
   (Static), `BoxCollider3D` (**IsSensor = true**), `LuaScript` =
   `character_controller/scripts/pickup.lua`. Right-click it →
   *Save as Prefab* → `character_controller/prefabs/pickup.lprefab`. Then
   drag the prefab into the arena several times at different positions.
9. **LevelExit** — Tag `Exit`. `MeshRendererComponent` (a marked cube/zone),
   `Rigidbody3D` (Static), `BoxCollider3D` (**IsSensor = true**),
   `LuaScript` = `character_controller/scripts/level_exit.lua`. Place it at
   one edge of the arena.
10. **Save** (`Ctrl+S`).

### Scripts (AI to write)

- `character_controller/scripts/character.lua` — WASD movement via
  `SetLinearVelocity3D`, Space jump via `ApplyImpulse3D`, and a camera-follow
  that tracks the player each frame.
- `character_controller/scripts/pickup.lua` — 3D sensor pickup;
  `OnSensorBegin` destroys the pickup (optionally bumps a counter).
- `character_controller/scripts/level_exit.lua` — sensor zone; on
  `OnSensorBegin` from the Player calls `Scene.Load("scenes/next_area.loom")`.

### Validation checklist

- [ ] WASD walks the capsule; Space makes it jump
- [ ] Camera follows the player smoothly
- [ ] Walking through a pickup makes it disappear (3D sensor event)
- [ ] Walls block the player (static BoxCollider3D)
- [ ] Entering the LevelExit zone loads `next_area`
- [ ] Pickups instantiated from `pickup.lprefab`

---

## Scene — `next_area.loom`

**Validates:** scene transitions end-to-end. Tiny destination scene reached
from `character_controller`.

**Theme:** a "you made it" message and a sensor zone that returns to
`character_controller`.

### Required assets

Reuses `common/models/` primitives and a `common/fonts/` font. No
scene-specific assets.

### Build steps

1. **New scene** → save as `assets/scenes/next_area.loom`.
2. **MainCamera** — Tag `MainCamera`, `CameraComponent` Perspective,
   **Primary = true**.
3. **Sun** — Tag `Sun`, `DirectionalLightComponent`, any downward angle.
4. **Banner** — Tag `Banner`. Add `TextComponent`: FontPath =
   `common/fonts/<your>.ttf`, Text = `You made it!`. Position it in front of
   the camera.
5. **Player** — same setup as `character_controller`'s Player
   (`MeshRendererComponent` capsule.glb, `Rigidbody3D` Dynamic FixedRotation,
   `CapsuleCollider3D`, `LuaScript` = the same `character.lua` copied into
   `next_area/scripts/`). Needed so the player can walk into the return zone.
   Also add a simple **Floor** (static cube + BoxCollider3D) so the player
   has something to stand on.
6. **ReturnZone** — Tag `Return`. `MeshRendererComponent`, `Rigidbody3D`
   (Static), `BoxCollider3D` (**IsSensor = true**), `LuaScript` =
   `next_area/scripts/return_zone.lua`.
7. **Save** (`Ctrl+S`).

### Scripts (AI to write)

- `next_area/scripts/return_zone.lua` — on `OnSensorBegin` from the Player,
  calls `Scene.Load("scenes/character_controller.loom")`.

### Validation checklist

- [ ] `character_controller`'s LevelExit loads this scene; the banner text renders
- [ ] Walking into the return zone loads `character_controller` again (round-trip works)

---

## Scene — `particle_lab.loom`

**Validates:** `ParticleComponent` — all three emitter shapes (Point / Box /
Circle) and both simulation spaces (World / Local). Pure visual; no scripts.

**Theme:** four side-by-side emitter presets.

### Required assets

| Asset | Path | Notes |
| --- | --- | --- |
| *Particle sprite* (optional) | `particle_lab/textures/particle.png` | Soft round particle; emitters render a 1×1 white quad when no texture is set |

### Build steps

All four emitters loop continuously — this scene has no scripts, and `Emitting`
is the only on/off. Velocity is a random box-range `[VelocityMin, VelocityMax]`;
the emitter **Shape** sets only the spawn *position*, so an outward "burst" is
approximated with a wide symmetric velocity range. `GravityScale` defaults to 0
— set it to 1 for particles that fall.

1. **New scene** → save as `assets/scenes/particle_lab.loom`.
2. **MainCamera** — Tag `MainCamera`. `Camera`: **Orthographic**,
   **Primary = true**, Size ≈ `18`. Position `(0, 0, 10)`.
3. **Fountain** (Point, World, gravity) — Create Entity, position `(-7,-4,0)`.
   `Particle`: Shape = **Point**, Space = **World**, SpawnRate `50`, Lifetime
   `1.2`–`1.8`, VelocityMin `(-1.5, 7)`, VelocityMax `(1.5, 10)`, Gravity
   `(0,-9.8)`, **GravityScale `1`**, ColorBegin `(0.6,0.85,1,1)`, ColorEnd
   `(0.2,0.4,1,0)`, SizeBegin `0.3`, SizeEnd `0.1`.
4. **Explosion** (Circle, World, outward) — Create Entity, position `(-2.5,0,0)`.
   `Particle`: Shape = **Circle**, ShapeSize `(0.5,0)`, Space = **World**,
   SpawnRate `60`, Lifetime `0.5`–`1.0`, VelocityMin `(-7,-7)`, VelocityMax
   `(7,7)`, ColorBegin `(1,0.8,0.2,1)`, ColorEnd `(1,0.1,0,0)`, SizeBegin
   `0.35`, SizeEnd `0`. (A true one-shot burst would need a script to flip
   `Emitting`; here it loops.)
5. **SmokePlume** (Box, World, rising + growing) — Create Entity, position
   `(2.5,-3,0)`. `Particle`: Shape = **Box**, ShapeSize `(0.6,0.2)`, Space =
   **World**, SpawnRate `25`, Lifetime `2.0`–`3.0`, VelocityMin `(-0.3,0.8)`,
   VelocityMax `(0.3,1.6)`, ColorBegin `(0.5,0.5,0.5,0.8)`, ColorEnd
   `(0.2,0.2,0.2,0)`, SizeBegin `0.3`, **SizeEnd `1.2`**.
6. **SparkleRing** (Circle, Local, spinning) — Create Entity, position `(7,0,0)`.
   `Particle`: Shape = **Circle**, ShapeSize `(1.5,0)`, Space = **Local**,
   SpawnRate `40`, Lifetime `1.0`–`1.5`, VelocityMin `(-0.2,-0.2)`, VelocityMax
   `(0.2,0.2)`, **RotationSpeed `3`**, ColorBegin `(1,1,0.6,1)`, ColorEnd
   `(1,0.5,1,0)`, SizeBegin `0.2`, SizeEnd `0`.
7. **Save** (`Ctrl+S`).

### Validation checklist

- [ ] Point / Box / Circle emitters each spawn in the expected region (a point, a flat box, a filled disc)
- [ ] Fountain particles arc up and fall back (GravityScale 1); the others ignore gravity
- [ ] Color and size animate over each particle's lifetime — Smoke visibly grows + fades to transparent
- [ ] Move the **SparkleRing** entity in the viewport: its Local-space particles travel with it; a World-space emitter's already-spawned particles stay put
- [ ] All four emitters loop continuously

---

## Future scenes (planned)

Stubs for scenes that unlock when their roadmap feature ships. Listed now so the
demo set has a known shape; do **not** build until the feature lands.

### `skeletal_character.loom` — *(Phase 8 — 3D Skeletal Animation)*

**Will validate:** glTF skinned-mesh import + GPU skinning + skeletal animation
clips. A rigged character cycling idle / walk / run, plus the
`character_controller` Player upgraded from a capsule to this rigged character.
Asset: a rigged glTF character (e.g. a Mixamo export or a Khronos sample like
`CesiumMan.glb`) under `common/models/`.

Post-processing / bloom (Phase 7) does **not** get its own scene — it extends
`material_gallery` (the rendering test bed) via its Hero-pedestal extension
point.

---

## Author workflow

1. The Game Developer builds the scene in Weaver following its **Build steps**.
2. The AI writes the Lua scripts listed under **Scripts** for that scene.
3. The Game Developer runs the **Validation checklist** in Play mode.
4. When the scene passes, tick its checkbox here and move to the next scene.

When a roadmap feature ships, consult the **Roadmap coverage** table to find
which scene to extend.

### Build progress

- [x] `platformer_2d.loom`
- [x] `material_gallery.loom`
- [x] `physics_playground.loom`
- [x] `character_controller.loom`
- [x] `next_area.loom`
- [x] `particle_lab.loom`
