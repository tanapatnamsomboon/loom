# Sandbox Scene Plan

Authoring queue for the sandbox project's demo scenes. Paused 2026-05-16 to work on engine-side animation and tilemap improvements; pick this back up after those land.

Convention: `NN_name.loom` so they sort numerically in the content browser.

---

## Scene 01 — `01_2d_sandbox.loom`

**Validates:** 2D sprites + sprite animation + tilemap + 2D physics (Rigidbody2D, BoxCollider2D, sensor pickups) + collision events + audio (player-attached + tag-resolved SFXPlayer pattern) + text + Lua scripting in one scene.

**Theme:** mini platformer — player jumps between platforms collecting spinning coins, score in HUD, ambient music.

### Asset wishlist

**Required:**
- `textures/player.png` — ~64x64 transparent sprite (any character).
- `textures/tileset.png` — small grid (16x16 or 32x32 per tile), needs at least "grass top" and "dirt" cells.
- `textures/platform.png` — single ~96x24 platform sprite (wood / stone).
- `textures/coin_sheet.png` — horizontal strip of 8 frames OR 4x2 grid (gold coin spin).
- `fonts/<anything>.ttf` — any TrueType font.

**Optional (scene works without, just less polished):**
- `textures/sky.png` — ~1024x768 background.
- `sounds/music.wav` — ~30s loop.
- `sounds/jump.wav` — ~0.2s.
- `sounds/pickup.wav` — ~0.2s.

Source suggestions: [Kenney.nl](https://kenney.nl), [OpenGameArt.org](https://opengameart.org), [itch.io free assets](https://itch.io/game-assets/free).

### Entity layout

| Entity | Components | Key settings |
|---|---|---|
| MainCamera | Transform, Camera | Pos `(0, 2, 10)`. Orthographic, size=12, Primary=true |
| Background *(optional)* | Transform, SpriteRenderer | `textures/sky.png`. Scale `(24, 14, 1)`. Pos `(0, 2, -1)` |
| Floor | Transform, Tilemap | Tilemap `textures/tileset.png`; Cols=20, Rows=2, TileW=TileH=1, SheetCols/Rows = your sheet's. Pos `(0, -4, 0)`. Tile array: row 0 = grass-top index, row 1 = dirt index |
| FloorCollider | Transform, Rigidbody2D (Static), BoxCollider2D | Pos `(0, -4, 0)`. Box half-extents `(10, 0.5)` |
| Platform_1 | Transform, SpriteRenderer, Rigidbody2D (Static), BoxCollider2D | `textures/platform.png`. Pos `(3, -2, 0)`. Scale `(2, 0.4, 1)`. Box half-extents `(1, 0.2)` |
| Platform_2 | same | Pos `(-3, -1, 0)` |
| Platform_3 | same | Pos `(0, 1, 0)` |
| Player | Transform, SpriteRenderer, Rigidbody2D (Dynamic, FixedRotation=true), BoxCollider2D, LuaScript, AudioSource | Tag=`Player`. `textures/player.png`. Pos `(-3, 2, 0)`. Box half-extents `(0.4, 0.4)`. Script=`scripts/player_2d.lua`. AudioSource=`sounds/jump.wav`, AutoPlay=false, Volume=0.6 |
| SFXPlayer | AudioSource | Tag=`SFXPlayer`. Path=`sounds/pickup.wav`. AutoPlay=false. Volume=0.7 |
| Coin_1 | Transform, SpriteRenderer, AnimationComponent, Rigidbody2D (Static), BoxCollider2D (IsSensor=true), LuaScript | `textures/coin_sheet.png`. 8-frame spin, FrameDuration=0.1s, Loop. Pos `(3, -1.4, 0)`. Box half-extents `(0.3, 0.3)`. Script=`scripts/collectible.lua` |
| Coin_2 | same | Pos `(-3, -0.4, 0)` |
| Coin_3 | same | Pos `(0, 1.6, 0)` |
| Score | Transform, TextComponent | Tag=`Score`. Pos `(-10, 6, 0)`. Font=`fonts/<your>.ttf`. Text=`"Score: 0"`. FontSize=0.6. White |
| Music *(optional)* | AudioSource | Path=`sounds/music.wav`. AutoPlay, Loop, Volume=0.3 |

### Scripts (already written in `scripts/`)

- `player_2d.lua` — WASD lateral + grounded-edge-detected jump. Properties: MoveSpeed=5, JumpImpulse=8.
- `collectible.lua` — sensor pickup; finds `Score` by tag and bumps the counter; finds `SFXPlayer` by tag and triggers it; then `entity:Destroy()`.

### Validation checklist

- [ ] Player falls and lands (gravity + collider)
- [ ] A/D moves laterally; Space jumps only when grounded
- [ ] Walking into a coin bumps `Score: N -> N+1` and the coin disappears
- [ ] Coin spin animation loops smoothly
- [ ] Pickup SFX plays after coin disappears (validates the SFXPlayer-by-tag pattern)
- [ ] Background music auto-plays from start
- [ ] Tilemap renders correctly behind / under the player

---

## Scene 02 — `02_3d_physics.loom`

**Validates:** mesh loading (GLTF/GLB) + Renderer3D + Blinn-Phong lights (directional + point) + Jolt physics with all three collider types (Box / Sphere / Capsule) + 3D collision events + collider debug rendering.

**Theme:** physics playground — a ramped floor with dynamic cubes / spheres / capsules raining down; a collision logger script prints contact events to the console.

**Asset wishlist:**
- `models/cube.glb` — unit cube, smooth-shaded.
- `models/sphere.glb` — UV sphere, ~400 tris.
- `models/capsule.glb` — capsule mesh (cylinder + 2 hemispheres).
- `textures/wood.png` or `textures/stone.png` — albedo texture for the floor.

**Scripts to write:**
- `scripts/collision_logger.lua` — `function OnCollisionBegin(other) Log.Info("hit " .. other:GetTag()) end`.

---

## Scene 03 — `03_3d_character.loom`

**Validates:** capsule rigidbody character controller + 3D Lua physics API (`SetLinearVelocity3D`, `ApplyImpulse3D`) + sensor pickups in 3D + scene transition (`Scene.Load`).

**Theme:** simple first-person-ish capsule that walks around a small arena, picks up sensor cubes, and walks into a "level exit" zone that loads scene 04.

**Asset wishlist:** reuses scene 02's meshes + a couple of small pickup meshes.

**Scripts to write:**
- `scripts/character_3d.lua` — WASD via `SetLinearVelocity3D`, Space jumps via `ApplyImpulse3D`, camera follow.
- `scripts/pickup.lua` — 3D-flavored `collectible.lua`.
- `scripts/level_exit.lua` — sensor zone that calls `Scene.Load("scenes/04_next_level.loom")` on enter.

---

## Scene 04 — `04_next_level.loom`

**Validates:** scene transitions end-to-end. Tiny destination scene reached from scene 03.

**Theme:** "you made it" message + a sensor back-trigger that returns to scene 03.

**Scripts to write:**
- `scripts/back_to_03.lua` — `Scene.Load("scenes/03_3d_character.loom")`.

---

## Scene 05 — `05_particles.loom`

**Validates:** `ParticleComponent` — all three emitter shapes (Point / Box / Circle) and both simulation spaces (World / Local).

**Theme:** four side-by-side emitter presets — fountain, explosion (one-shot), smoke plume, sparkle ring. Pure visual; no scripts.

---

## Author workflow

User builds each scene in Weaver based on the layout table; AI writes the Lua scripts the scene needs. After each scene saves and validates, mark its checkbox here and move to the next.
