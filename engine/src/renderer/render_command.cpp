#include "loom/renderer/render_command.h"
#include "platform/opengl/opengl_renderer_api.h"

namespace Loom {

    std::unique_ptr<RendererAPI> RenderCommand::sRendererAPI = std::make_unique<OpenGLRendererAPI>();

}