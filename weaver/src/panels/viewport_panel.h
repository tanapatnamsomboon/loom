#pragma once

#include "editor_context.h"
#include <loom/core/timestep.h>
#include <loom/renderer/buffer.h>
#include <loom/renderer/framebuffer.h>
#include <loom/renderer/shader.h>
#include <loom/renderer/vertex_array.h>

namespace Weaver {

    class ViewportPanel {
    public:
        explicit ViewportPanel(EditorContext& ctx);

        void Init();

        void BeginFrame();
        void RenderScene(Loom::Timestep ts);
        void UpdateHoveredEntity();
        void EndFrame();

        void OnImGuiRender();

    private:
        void HandleViewportResize();
        void UpdateViewportBounds();
        void UpdateViewportSize();
        void RenderGizmos();

        EditorContext& mContext;

        std::shared_ptr<Loom::Framebuffer>  mFramebuffer;

        std::shared_ptr<Loom::VertexArray>  mSkyboxVAO;
        std::shared_ptr<Loom::VertexBuffer> mSkyboxVBO;
        std::shared_ptr<Loom::Shader>       mSkyboxShader;

        std::shared_ptr<Loom::VertexArray>  mGridVAO;
        std::shared_ptr<Loom::VertexBuffer> mGridVBO;
        std::shared_ptr<Loom::Shader>       mGridShader;
    };

} // namespace Weaver