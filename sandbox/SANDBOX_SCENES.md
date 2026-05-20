# Sandbox Scene Plan

Authoring queue for the sandbox project's demo scenes. Each scene dogfoods a
slice of the engine end-to-end; together they are the validation gate before
Phase 6. Rewritten 2026-05-21 to match the current engine (PBR + IBL, CSM
shadows, scene-owned environment, tilemap per-tile collision, 3D physics,
prefabs) and to design the scenes as **living showcases** — each one carries
explicit extension points so future-roadmap features slot into the *same*
scene instead of needing a rewrite.

Convention: `NN_name.loom` so they sort numerically in the content browser.

**Workflow:** the Game Developer builds each scene in Weaver from the layout
table; the AI writes the Lua scripts the scene needs. After a scene saves and
validates, tick its checkbox and move to the next.

---

## Roadmap coverage

Which scene exercises which engine feature — so coverage gaps stay visible and,
when a roadmap feature ships, it's obvious which scene to extend.

| Feature | Status | Scene(s) |
|---|---|---|
| 2D sprites + sprite animation | shipped | 01 |
| Tilemap + per-tile collision | shipped | 01 |
| 2D physics (Box2D, sensors, events) | shipped | 01 |
| Audio (attached + tag-resolved SFX) | shipped | 01 |
| Text / HUD | shipped | 01, 05 |
| Lua scripting | shipped | 01, 03, 04, 05 |
| **Prefabs** (`.lprefab`, drag-instantiate, `Instantiate`) | shipped | 01 (coins), 04 (pickups) |
| Mesh loading (glTF/GLB) | shipped | 02, 03, 04, 05 |
| PBR materials (factor-based) | shipped | 02, 03, 04 |
| IBL / HDR skybox (Scene Properties) | shipped | 02, 03 |
| CSM shadows | shipped | 02, 03, 04 |
| 3D physics (Jolt — Box/Sphere/Capsule) | shipped | 03, 04 |
| Scene transitions | shipped | 04 ↔ 05 |
| Particle system | shipped | 06 |
| Material texture maps (metalRough/normal/AO/emissive) | Phase 5 follow-up | **02** — Hero pedestal *(extend)* |
| Post-processing / bloom | Phase 7 | **02** — *(extend)* |
| 3D skeletal animation | Phase 8 | **07** *(planned)*, **04** Player *(mesh swap)* |

---

## Scene 01 — `01_2d_platformer.loom`

**Validates:** 2D sprites + sprite animation + tilemap with **per-tile
collision** + 2D physics (Rigidbody2D, BoxCollider2D, sensor pickups) +
collision events + audio (player-attached + tag-resolved SFXPlayer pattern) +
text + Lua scripting + **prefabs** — the whole 2D stack in one scene.

**Theme:** mini platformer — player jumps between platforms collecting spinning
coins, score in the HUD, ambient music.

### Asset wishlist

**Required:**
- `textures/player_sheet.png` — character **sprite sheet** with idle + run frames (jump frame optional); the Sprite Animator picks frames from it.
- `textures/tileset.png` — small grid (16×16 or 32×32 per tile); needs at least
  a "grass top" and a "dirt" cell.
- `textures/platform.png` — single ~96×24 platform sprite (wood / stone).
- `textures/coin_sheet.png` — horizontal strip of 8 frames (gold coin spin).
- `fonts/<anything>.ttf` — any TrueType font.

**Optional (scene works without, just less polished):**
- `textures/sky.png` — ~1024×768 background.
- `sounds/music.wav` — ~30s loop.
- `sounds/jump.wav` / `sounds/pickup.wav` — ~0.2s each.

Source suggestions: [Kenney.nl](https://kenney.nl),
[OpenGameArt.org](https://opengameart.org),
[itch.io free assets](https://itch.io/game-assets/free).

### Entity layout

| Entity | Components | Key settings |
|---|---|---|
| MainCamera | Transform, Camera | Pos `(0, 2, 10)`. Orthographic, size ≈ 12, Primary = true |
| Background *(optional)* | Transform, SpriteRenderer | `textures/sky.png`. Scale `(24, 14, 1)`. Pos `(0, 2, -1)` |
| Floor | Transform, Tilemap | `textures/tileset.png`; Columns = 20, Rows = 2, TileW = TileH = 1, SheetCols/Rows = your sheet's. Pos `(0, -4, 0)`. Row 0 = grass-top tile, row 1 = dirt tile. **Shift+click the grass + dirt tiles in the palette to mark them Solid** — the tilemap generates its own static collider, no separate floor entity needed |
| Platform_1 | Transform, SpriteRenderer, Rigidbody2D (Static), BoxCollider2D | `textures/platform.png`. Pos `(3, -2, 0)`. Scale `(2, 0.4, 1)`. Box half-extents `(1, 0.2)` |
| Platform_2 | same | Pos `(-3, -1, 0)` |
| Platform_3 | same | Pos `(0, 1, 0)` |
| Player | Transform, SpriteRenderer, **Animation**, Rigidbody2D (Dynamic, FixedRotation = true), BoxCollider2D, LuaScript, AudioSource | Tag = `Player`. `textures/player_sheet.png`. Animation clips named **`idle` / `run` / `jump`** (the script drives them). Pos `(-3, 2, 0)`. Box half-extents `(0.4, 0.4)`. Script = `scripts/player_2d.lua`. AudioSource = `sounds/jump.wav`, AutoPlay = false, Volume = 0.6 |
| SFXPlayer | AudioSource | Tag = `SFXPlayer`. AssetPath = `sounds/pickup.wav`. AutoPlay = false. Volume = 0.7 |
| Coin_1 … Coin_3 | Transform, SpriteRenderer, Animation, Rigidbody2D (Static), BoxCollider2D (IsSensor = true), LuaScript | See **prefab** note below. `textures/coin_sheet.png`. 8-frame spin clip, ~0.1s/frame, looping. Box half-extents `(0.3, 0.3)`. Script = `scripts/collectible.lua`. Positions `(3, -1.4, 0)`, `(-3, -0.4, 0)`, `(0, 1.6, 0)` |
| Score | Transform, Text | Tag = `Score`. Pos `(-10, 6, 0)`. Font = `fonts/<your>.ttf`. Text = `"Score: 0"`. FontSize ≈ 0.6. White |
| Music *(optional)* | AudioSource | AssetPath = `sounds/music.wav`. AutoPlay, Loop, Volume = 0.3 |

**Prefab note:** build one fully-configured Coin entity, then **Save as Prefab**
(`prefabs/coin.lprefab`) and drag-instantiate it from the content browser for
the other two. This dogfoods the prefab pipeline; the spinning-coin prefab is
also reusable in later scenes.

### Scripts (AI to write)

- `player_2d.lua` — WASD lateral + grounded-edge-detected jump. MoveSpeed ≈ 5,
  JumpImpulse ≈ 8.
- `collectible.lua` — sensor pickup; finds `Score` by tag and bumps the counter,
  finds `SFXPlayer` by tag and triggers it, then `entity:Destroy()`.

### Validation checklist

- [ ] Player falls and lands on the tilemap floor (tilemap per-tile collision)
- [ ] A/D moves laterally; Space jumps only when grounded
- [ ] Player animates: `idle` when still, `run` when moving, `jump` when airborne; sprite flips to face the move direction
- [ ] Player lands and stands on each platform (BoxCollider2D)
- [ ] Walking into a coin bumps `Score: N → N+1` and the coin disappears
- [ ] Coin spin animation loops smoothly
- [ ] Pickup SFX plays after the coin disappears (SFXPlayer-by-tag pattern)
- [ ] Background music auto-plays from start
- [ ] All three coins instantiated from `coin.lprefab`

---

## Scene 02 — `02_pbr_showcase.loom`

**Validates:** the whole Phase 5 rendering pipeline — metallic-roughness PBR,
image-based lighting from a scene-owned HDR environment, and cascaded shadow
maps. Pure visual; no scripts.

**Theme:** a material showcase — a grid of spheres sweeping Roughness and
Metallic under an HDR sky, all casting shadows onto a ground plane.

> **Extension points (this is the rendering test bed):**
> - **Hero pedestal** — reserved front-center slot. *Today:* a plain PBR
>   sphere. *Phase 5 follow-up:* swap to a fully-textured model (DamagedHelmet)
>   once metalRoughness / normal / AO / emissive maps land. *Phase 7:* an
>   emissive object beside it becomes the bloom reference.
> - Keep the sphere grid as labelled rows so a **normal-mapped row** can be
>   appended later without disturbing the existing sweep.

### Asset wishlist

**Required:**
- `models/sphere.glb` — smooth-shaded UV sphere.
- `models/plane.glb` — a ground plane (a flattened cube works too).
- `environments/<name>.hdr` — an equirectangular HDR for the skybox + IBL.

Source suggestions: [Poly Haven](https://polyhaven.com/hdris) (HDRIs),
[Khronos glTF Sample Models](https://github.com/KhronosGroup/glTF-Sample-Assets)
(meshes).

### Scene setup

1. Open the **Scene Properties** panel (View → Scene Properties) and set the
   **Skybox HDR** to your `environments/<name>.hdr`. This drives the skybox,
   diffuse irradiance, and specular reflections for every PBR surface.
2. Build the entity layout below.

### Entity layout

| Entity | Components | Key settings |
|---|---|---|
| MainCamera | Transform, Camera | Pos `(0, 1.5, 9)`, tilted slightly down. **Perspective** projection, Primary = true |
| Sun | Transform, DirectionalLight | Rotate so it shines down at ~45° (light points along the entity's local −Z). Intensity ≈ 2.0 |
| Ground | Transform, MeshRenderer | `models/plane.glb`. Scale `(20, 1, 20)`. Pos `(0, -1.5, 0)`. Roughness ≈ 0.9, Metallic = 0 |
| Spheres | Transform, MeshRenderer | `models/sphere.glb`. A **5 × 2 grid**, spacing ≈ 2.5 units. **Columns** sweep Roughness `0.0 / 0.25 / 0.5 / 0.75 / 1.0`; **bottom row** Metallic = 0 (dielectric), **top row** Metallic = 1 (metal). Give the metal row a tinted Albedo (e.g. gold `(1.0, 0.78, 0.34)`) so the metallic tint reads clearly |
| HeroPlinth | Transform, MeshRenderer | A small flattened cube, front-center, raised. Roughness ≈ 0.6, Metallic = 0 |
| Hero | Transform, MeshRenderer | `models/sphere.glb` on top of the plinth — **the reserved extension slot** (see note above). Mid-roughness dielectric for now |

### Validation checklist

- [ ] Skybox renders behind the spheres (set via Scene Properties)
- [ ] Low-roughness spheres show sharp environment reflections; high-roughness
      spheres look matte — a clean visual sweep across each row
- [ ] Metal row reflects the environment and tints by Albedo; dielectric row
      keeps a diffuse look with a tighter specular highlight
- [ ] Every sphere casts a shadow onto the ground plane (CSM)
- [ ] Enter Play — with a Skybox HDR assigned, the runtime looks identical to
      edit mode; clear the Skybox path and Play goes dark (no fallback in
      runtime, by design)

---

## Scene 03 — `03_3d_physics.loom`

**Validates:** mesh loading (GLTF/GLB) + Renderer3D PBR + Jolt 3D physics with
all three collider types (Box / Sphere / Capsule) + 3D collision events +
collider debug rendering.

**Theme:** physics playground — dynamic cubes, spheres, and capsules raining
onto a tilted floor; a collision logger script prints contact events.

### Asset wishlist

- `models/cube.glb`, `models/sphere.glb`, `models/capsule.glb` — primitive meshes.
- `textures/wood.png` or `textures/stone.png` — albedo for the floor (optional;
  factors-only also fine).
- Reuses Scene 02's HDR — set the Skybox in Scene Properties so the props are lit.

### Entity layout

| Entity | Components | Key settings |
|---|---|---|
| MainCamera | Transform, Camera | Pos `(0, 6, 14)`, tilted down. Perspective, Primary = true |
| Sun | Transform, DirectionalLight | Angled downward. Intensity ≈ 2.0 |
| Floor | Transform, MeshRenderer, Rigidbody3D (Static), BoxCollider3D | `models/cube.glb`. Scale `(12, 0.5, 12)`, rotated ~10° so props slide. BoxCollider3D HalfExtents matched to the scaled mesh |
| Cube_* | Transform, MeshRenderer, Rigidbody3D (Dynamic), BoxCollider3D, LuaScript | `models/cube.glb`. Tag = `Cube`. Dropped from ~`(x, 8, z)`. Script = `scripts/collision_logger.lua` |
| Sphere_* | Transform, MeshRenderer, Rigidbody3D (Dynamic), SphereCollider3D, LuaScript | `models/sphere.glb`. Tag = `Sphere`. SphereCollider3D Radius matched to the mesh |
| Capsule_* | Transform, MeshRenderer, Rigidbody3D (Dynamic), CapsuleCollider3D, LuaScript | `models/capsule.glb`. Tag = `Capsule`. CapsuleCollider3D Radius + HalfHeight matched to the mesh |

Drop ~3–4 of each primitive at staggered heights/positions so they collide on
the way down.

### Scripts (AI to write)

- `collision_logger.lua` — `OnCollisionBegin(other)` / `OnCollisionEnd(other)`
  log the other entity's tag via `Log.Info`.

### Validation checklist

- [ ] All props fall under gravity and rest on / slide down the tilted floor
- [ ] Box, Sphere, and Capsule colliders all behave correctly (no
      interpenetration, capsules roll/tip plausibly)
- [ ] Console + `loom.log` show collision-begin/end lines as props settle
- [ ] Collider debug overlay (toggle in settings) draws each shape correctly
- [ ] PBR meshes are lit by the HDR environment

---

## Scene 04 — `04_3d_character.loom`

**Validates:** capsule rigidbody character controller + 3D Lua physics API
(`SetLinearVelocity3D`, `ApplyImpulse3D`) + sensor pickups in 3D + camera
follow + scene transition (`Scene.Load`) + **prefabs**.

**Theme:** a capsule character walks around a small arena, collects sensor
pickups, and steps into a "level exit" zone that loads Scene 05.

> **Extension point:** the **Player** entity is the reserved slot for Phase 8
> 3D skeletal animation — when skinned meshes land, swap the capsule mesh for a
> rigged character with walk / idle / jump clips; the controller script and
> physics body stay as-is, no scene restructuring.

### Asset wishlist

Reuses Scene 03's primitive meshes + Scene 02's HDR. Pickups can be small
scaled cubes/spheres.

### Entity layout

| Entity | Components | Key settings |
|---|---|---|
| MainCamera | Transform, Camera | Perspective, Primary = true. Positioned behind/above the player; `character_3d.lua` drives the follow |
| Sun | Transform, DirectionalLight | Angled downward |
| Floor | Transform, MeshRenderer, Rigidbody3D (Static), BoxCollider3D | Wide flat cube arena |
| Walls | Transform, MeshRenderer, Rigidbody3D (Static), BoxCollider3D | A few cubes ringing the arena |
| Player | Transform, MeshRenderer, Rigidbody3D (Dynamic, FixedRotation = true), CapsuleCollider3D, LuaScript | Tag = `Player`. `models/capsule.glb`. Script = `scripts/character_3d.lua` |
| Pickup_* | Transform, MeshRenderer, Rigidbody3D (Static), BoxCollider3D (IsSensor = true), LuaScript | Tag = `Pickup`. Script = `scripts/pickup_3d.lua`. **Build one, Save as Prefab (`prefabs/pickup.lprefab`), drag-instantiate the rest** |
| LevelExit | Transform, MeshRenderer, Rigidbody3D (Static), BoxCollider3D (IsSensor = true), LuaScript | Tag = `Exit`. Script = `scripts/level_exit.lua` |

### Scripts (AI to write)

- `character_3d.lua` — WASD movement via `SetLinearVelocity3D`, Space jump via
  `ApplyImpulse3D`, and a camera-follow that tracks the player each frame.
- `pickup_3d.lua` — 3D sensor pickup; `OnSensorBegin` destroys the pickup
  (optionally bumps a counter).
- `level_exit.lua` — sensor zone; on `OnSensorBegin` from the Player calls
  `Scene.Load("scenes/05_next_level.loom")`.

### Validation checklist

- [ ] WASD walks the capsule; Space makes it jump
- [ ] Camera follows the player smoothly
- [ ] Walking through a pickup makes it disappear (3D sensor event)
- [ ] Walls block the player (static BoxCollider3D)
- [ ] Entering the LevelExit zone loads Scene 05
- [ ] Pickups instantiated from `pickup.lprefab`

---

## Scene 05 — `05_next_level.loom`

**Validates:** scene transitions end-to-end. Tiny destination scene reached
from Scene 04.

**Theme:** a "you made it" message and a sensor zone that returns to Scene 04.

### Entity layout

| Entity | Components | Key settings |
|---|---|---|
| MainCamera | Transform, Camera | Perspective, Primary = true |
| Sun | Transform, DirectionalLight | Any angle |
| Banner | Transform, Text | Text = `"You made it!"`. Font = `fonts/<your>.ttf` |
| ReturnZone | Transform, MeshRenderer, Rigidbody3D (Static), BoxCollider3D (IsSensor = true), LuaScript | Script = `scripts/back_to_04.lua` |
| Player | (same setup as Scene 04's Player) | So the player can walk into the return zone |

### Scripts (AI to write)

- `back_to_04.lua` — on `OnSensorBegin` calls
  `Scene.Load("scenes/04_3d_character.loom")`.

### Validation checklist

- [ ] Scene 04's LevelExit loads this scene; the banner text renders
- [ ] Walking into the return zone loads Scene 04 again (round-trip works)

---

## Scene 06 — `06_particles.loom`

**Validates:** `ParticleComponent` — all three emitter shapes (Point / Box /
Circle) and both simulation spaces (World / Local). Pure visual; no scripts.

**Theme:** four side-by-side emitter presets.

### Asset wishlist

- `textures/particle.png` — a soft round particle (optional; emitters render a
  1×1 white quad when no texture is set).

### Entity layout

| Entity | Components | Key settings |
|---|---|---|
| MainCamera | Transform, Camera | Orthographic, sized to frame all four emitters. Primary = true |
| Fountain | Transform, Particle | Shape = Point, Space = World, upward VelocityMin/Max, GravityScale ≈ 1, looping |
| Explosion | Transform, Particle | Shape = Circle, Space = World, radial velocity, one-shot (toggle Emitting off after a burst, or low SpawnRate) |
| SmokePlume | Transform, Particle | Shape = Box, Space = World, slow upward velocity, ColorBegin → ColorEnd fade to transparent, large SizeEnd |
| SparkleRing | Transform, Particle | Shape = Circle, Space = Local, RotationSpeed > 0 so the ring spins with the entity |

### Validation checklist

- [ ] Each emitter shape spawns particles in the expected region
- [ ] World-space particles stay put when the emitter entity moves;
      Local-space particles move with it
- [ ] Color / size animate over particle lifetime
- [ ] The one-shot explosion fires a single burst, the others loop

---

## Future scenes (planned)

Stubs for scenes that unlock when their roadmap feature ships. Listed now so the
demo set has a known shape; do **not** build until the feature lands.

### `07_skeletal_anim.loom` — *(Phase 8 — 3D Skeletal Animation)*

**Will validate:** glTF skinned-mesh import + GPU skinning + skeletal animation
clips. A rigged character cycling idle / walk / run, plus the Scene 04 Player
upgraded from a capsule to this rigged character. Asset: a rigged glTF
character (e.g. a Mixamo export or a Khronos sample like `CesiumMan.glb`).

Post-processing / bloom (Phase 7) does **not** get its own scene — it extends
Scene 02 (the rendering test bed) via its Hero-pedestal extension point.

---

## Author workflow

User builds each scene in Weaver from the layout table; AI writes the Lua
scripts the scene needs. After each scene saves and validates, tick its
checkbox and move to the next. When a roadmap feature ships, consult the
**Roadmap coverage** table to find which scene to extend.
