#include "loom/renderer/render_pipeline.h"
#include "loom/core/log.h"
#include "loom/renderer/render_command.h"
#include "loom/renderer/renderer_3d.h"

namespace Loom {

    RenderPipeline::RenderPipeline(const RenderPipelineConfig& cfg)
        : mConfig(cfg) {
        FramebufferSpecification hdr_spec;
        hdr_spec.Attachments.Attachments.push_back(FramebufferTextureFormat::RGBA16F);
        if (mConfig.IncludePickingAttachment)
            hdr_spec.Attachments.Attachments.push_back(FramebufferTextureFormat::RED_INTEGER);
        hdr_spec.Attachments.Attachments.push_back(FramebufferTextureFormat::DEPTH24STENCIL8);
        hdr_spec.Width  = 1280;
        hdr_spec.Height = 720;
        mHDR = Framebuffer::Create(hdr_spec);

        FramebufferSpecification ldr_spec;
        ldr_spec.Attachments = { FramebufferTextureFormat::RGBA8 };
        ldr_spec.Width  = 1280;
        ldr_spec.Height = 720;
        mLDR   = Framebuffer::Create(ldr_spec);
        mFinal = Framebuffer::Create(ldr_spec);
    }

    void RenderPipeline::Resize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) return;
        const auto& spec = mHDR->GetSpecification();
        if (spec.Width == width && spec.Height == height) return;

        mHDR  ->Resize(width, height);
        mLDR  ->Resize(width, height);
        mFinal->Resize(width, height);
    }

    uint32_t RenderPipeline::GetWidth()  const { return mHDR->GetSpecification().Width;  }
    uint32_t RenderPipeline::GetHeight() const { return mHDR->GetSpecification().Height; }

    void RenderPipeline::BeginScene() {
        mHDR->Bind();
        RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        RenderCommand::Clear();
        if (mConfig.IncludePickingAttachment)
            mHDR->ClearAttachment(1, -1); // -1 = "no entity"; matches Renderer3D's null sentinel
    }

    void RenderPipeline::EndScene() {
        mHDR->Unbind();

        const auto& hdr_spec = mHDR->GetSpecification();
        Renderer3D::BloomPass(mHDR->GetColorAttachmentRendererID(0),
                              hdr_spec.Width, hdr_spec.Height);

        mLDR->Bind();
        RenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        RenderCommand::Clear();
        Renderer3D::Tonemap(mHDR->GetColorAttachmentRendererID(0));
        mLDR->Unbind();

        if (Renderer3D::IsFXAAEnabled()) {
            mFinal->Bind();
            RenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            RenderCommand::Clear();
            const auto& final_spec = mFinal->GetSpecification();
            Renderer3D::FXAAPass(mLDR->GetColorAttachmentRendererID(0),
                                 final_spec.Width, final_spec.Height);
            mFinal->Unbind();
        }
    }

    uint32_t RenderPipeline::GetFinalColorTextureID() const {
        return Renderer3D::IsFXAAEnabled()
            ? mFinal->GetColorAttachmentRendererID(0)
            : mLDR  ->GetColorAttachmentRendererID(0);
    }

    uint32_t RenderPipeline::GetHDRColorTextureID() const {
        return mHDR->GetColorAttachmentRendererID(0);
    }

    int RenderPipeline::ReadPickingPixel(int x, int y) const {
        if (!mConfig.IncludePickingAttachment) return -1;
        const auto& spec = mHDR->GetSpecification();
        if (x < 0 || y < 0 || x >= (int)spec.Width || y >= (int)spec.Height) return -1;
        return mHDR->ReadPixel(1, x, y);
    }

    Framebuffer& RenderPipeline::GetHDRFramebuffer() { return *mHDR; }

} // namespace Loom
