#pragma once

#include "loom/core/core.h"
#include "loom/renderer/framebuffer.h"
#include <memory>

namespace Loom {

    // Configuration for a RenderPipeline instance.
    struct RenderPipelineConfig {
        // When true, the HDR framebuffer gains a RED_INTEGER attachment (slot 1)
        // for entity-id picking. Editor uses this; runtime leaves it off so the
        // packed attachment doesn't cost VRAM in a shipped game.
        bool IncludePickingAttachment = false;
    };

    // Owns the HDR scene framebuffer + post-process chain (Bloom -> Tonemap ->
    // FXAA) so the editor's viewport and the standalone runtime can share one
    // implementation. Bloom mip chain still lives on Renderer3D's static storage
    // (one process = one pipeline = no thrash); this class drives the orchestration.
    //
    // Typical use:
    //   pipeline.Resize(width, height);
    //   pipeline.BeginScene();
    //   scene->OnUpdateRuntime(ts);   // (or OnUpdateEditor)
    //   pipeline.EndScene();
    //   uint32_t final_tex = pipeline.GetFinalColorTextureID();
    //   // editor: ImGui::Image(final_tex); runtime: glBlitFramebuffer to default FB
    class LOOM_API RenderPipeline {
    public:
        explicit RenderPipeline(const RenderPipelineConfig& cfg = {});

        // Resizes all internal framebuffers (HDR + LDR + Final) to match.
        // No-op when the requested size equals the current size.
        void Resize(uint32_t width, uint32_t height);

        uint32_t GetWidth()  const;
        uint32_t GetHeight() const;

        // Binds the HDR framebuffer + clears color and the EID attachment
        // (when picking is enabled) to -1.
        void BeginScene();

        // Runs Bloom -> Tonemap -> FXAA on whatever was rendered into the HDR
        // framebuffer. Leaves no FB bound on exit. After this call,
        // GetFinalColorTextureID() returns the displayable result.
        void EndScene();

        // Texture ID the caller samples or blits to display.
        // FXAA on -> Final FB color; FXAA off -> LDR FB color.
        uint32_t GetFinalColorTextureID() const;

        // Direct access to the HDR scene color (for special editor viz that needs
        // pre-tonemap radiance values).
        uint32_t GetHDRColorTextureID() const;

        // Editor-only entity picking. Returns -1 when picking is disabled or the
        // coordinates fall outside the framebuffer. x/y in physical framebuffer
        // pixels with the origin in the top-left.
        int ReadPickingPixel(int x, int y) const;

        // Direct access to the HDR framebuffer for code paths that still need
        // to bind it (e.g. mid-frame UI overlay drawn into the scene buffer).
        Framebuffer& GetHDRFramebuffer();

    private:
        RenderPipelineConfig            mConfig;
        std::shared_ptr<Framebuffer>    mHDR;    // RGBA16F + (optional) RED_INTEGER + DEPTH24STENCIL8
        std::shared_ptr<Framebuffer>    mLDR;    // RGBA8 — tonemap output
        std::shared_ptr<Framebuffer>    mFinal;  // RGBA8 — FXAA output
    };

} // namespace Loom
