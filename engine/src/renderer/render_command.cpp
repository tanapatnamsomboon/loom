#include "loom/renderer/render_command.h"
#include "platform/opengl/opengl_renderer_api.h"

namespace Loom {

    RendererAPI* RenderCommand::sRendererAPI = new OpenGLRendererAPI();

}