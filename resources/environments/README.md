# Engine Default Environment

Drop a 2:1 equirectangular HDR file here named **`default.hdr`** to make it the
engine-wide default skybox / IBL environment. Every scene that doesn't set its
own `SkyboxPath` will use this one.

- File format: Radiance RGBE `.hdr` (loaded via `stbi_loadf`).
- Recommended source: <https://polyhaven.com/hdris> (CC0).
- Recommended resolution: 2048×1024 or 4096×2048.
- The file is copied to `build/<preset>/bin/resources/environments/default.hdr`
  on build via the existing `CopyWeaverAssets` / engine-asset copy step.

Per-scene overrides win — set a scene's `SkyboxPath` from the toolbar's
**Settings → SCENE — SKYBOX** popup and it'll replace the default for that scene.
