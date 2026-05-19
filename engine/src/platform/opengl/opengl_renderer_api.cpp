#include "platform/opengl/opengl_renderer_api.h"
#include "loom/core/log.h"
#include <glad/glad.h>

namespace Loom {

    void OpenGLRendererAPI::Init() {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_DEPTH_TEST);

        glEnable(GL_MULTISAMPLE);

        // Seamless cubemap sampling: blends across face borders instead of
        // clamping each face independently. Without this, low-res cubemaps
        // (notably the irradiance map) show visible 1-2 texel seams at
        // every face boundary — which read as "pixelated reflections" on a
        // smooth sphere because the sphere's normal sweeps across cube faces.
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        // Confirm the state actually took (a few drivers silently no-op this
        // enable; logging it makes the IBL "I see pixelated seams" case
        // diagnosable without a debugger).
        GLboolean seamless = glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        LOOM_CORE_TRACE("OpenGLRendererAPI::Init — GL_TEXTURE_CUBE_MAP_SEAMLESS={}",
                        seamless ? "ON" : "OFF");
    }

    void OpenGLRendererAPI::SetClearColor(float r, float g, float b, float a) {
        glClearColor(r, g, b, a);
    }

    void OpenGLRendererAPI::Clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::DrawIndexed(VertexArray* vertex_array, uint32_t index_count) {
        vertex_array->Bind();
        uint32_t count = index_count ? index_count : vertex_array->GetIndexBuffer()->GetCount();
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
    }

    void OpenGLRendererAPI::DrawLines(VertexArray* vertex_array, uint32_t vertex_count) {
        vertex_array->Bind();
        glDrawArrays(GL_LINES, 0, vertex_count);
    }

} // namespace Loom
