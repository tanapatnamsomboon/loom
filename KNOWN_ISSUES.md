# Known Issues

Live tracker for problems, bugs, design gaps, and follow-ups noticed during work. Entries can be deleted once fixed, finished, or invalidated.

---

## WeaverRuntime missing post-process chain

**Severity:** high — every shipped game renders wrong.

`RuntimeLayer::OnUpdate` calls `Scene::OnUpdateRuntime` straight to the window's default RGBA8 framebuffer. The editor's full pipeline (HDR RGBA16F scene FB → Bloom → ACES Tonemap → LDR FB → FXAA → final) lives on `ViewportPanel::EndFrame` and never runs in the runtime.

**Symptoms:**
- 2D scenes look dark (e.g. `platformer_2d`): `quad.frag` / `circle.frag` / `line.frag` / `grid.frag` apply `pow(rgb, 2.2)` linearization expecting a downstream tonemap+gamma re-encode that never happens → linear values shown on sRGB display.
- PBR meshes write linear HDR to an 8-bit framebuffer → values > 1 clamp, contrast collapses.
- Bloom and FXAA never apply regardless of state.

**Fix scope:** promote the editor's post chain into a place `RuntimeLayer` can share — either a `RenderPipeline` helper or onto `Scene` itself. `Renderer3D` already owns Bloom/Tonemap/FXAA; only the HDR framebuffer + chain orchestration need to move.

---

## FXAA / bloom toggles are global state, leak into Play mode

**Severity:** medium — confusing UX, not data-corrupting.

Toolbar → Settings → POST PROCESSING → FXAA (and the bloom-related setters) call `Renderer3D::SetFXAAEnabled` / `SetBloomEnabled` etc., which are pure process-global runtime flags. In the editor process, toggling them also changes what Play mode shows — the user expects these to be editor-preview-only.

**Fix scope:** Phase 9 stretch goal — "Editor-local override." Project baseline (`GraphicsConfig.FXAAEnabled`, `BloomEnabled` etc.) + editor-only override layered on top. Today there's no split.

---

## Settings-scope mishmash (project / scene / camera / global)

**Severity:** low — known inconsistency, Phase 9 is straightening it out.

| Setting | Scope today | Intended |
|---|---|---|
| Shadow map size, max distance | Project (✓ Phase 9 Slice 1) | Project baseline + scene override |
| Skybox / IBL env path | Scene | Scene |
| Aspect ratio (`FixedAspectRatio` + `AspectRatio`) | Camera | Camera |
| FXAA / bloom enabled / threshold / intensity | **Global static** | Project baseline + editor override |
| Mesh debug viz, skybox debug source | Scene + editor-only | Editor-only |

Conceptual model going forward: Project (baseline) → Scene (override) → Camera (volume-style local override) + editor-local override layer for preview. Phase 9 ships bottom-up.

---

## Sandbox `sandbox.loomproj` references missing `01_2d_sandbox.loom`

**Severity:** low — broken start scene, makes WeaverRuntime fail to load on first launch.

`StartScene: "scenes/01_2d_sandbox.loom"` does not exist on disk. Actual scene files in `sandbox/assets/scenes/` are `platformer_2d.loom`, `character_controller.loom`, `material_gallery.loom`, `next_area.loom`, `particle_lab.loom`, `physics_playground.loom`.

**Fix:** File → Project Settings → Start Scene → pick an existing scene → Apply (now persists to disk after the Apply-persistence fix in this session).
