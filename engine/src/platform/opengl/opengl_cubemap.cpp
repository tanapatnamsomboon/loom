#include "platform/opengl/opengl_cubemap.h"
#include "loom/asset/asset_manager.h"
#include "loom/core/log.h"
#include "loom/project/project.h"
#include "loom/renderer/shader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Loom {

    OpenGLTextureCubemap::OpenGLTextureCubemap(uint32_t face_size, CubemapFormat format, uint32_t mip_levels)
        : mFaceSize(face_size), mMipLevels(mip_levels), mFormat(format) {
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &mRendererID);

        GLenum gl_format = (format == CubemapFormat::RGB16F) ? GL_RGB16F : GL_RGBA8;
        glTextureStorage2D(mRendererID, mip_levels, gl_format, face_size, face_size);

        glTextureParameteri(mRendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(mRendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(mRendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        // Trilinear by default so prefilter mip-chain sampling looks correct in
        // mesh.frag (caller can override by re-setting params on the returned id).
        glTextureParameteri(mRendererID, GL_TEXTURE_MIN_FILTER,
                            mip_levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTextureParameteri(mRendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    OpenGLTextureCubemap::~OpenGLTextureCubemap() {
        glDeleteTextures(1, &mRendererID);
    }

    void OpenGLTextureCubemap::Bind(uint32_t slot) const {
        glBindTextureUnit(slot, mRendererID);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Static factories on TextureCubemap (lives here, OpenGL-only for now)
    // ────────────────────────────────────────────────────────────────────────

    std::shared_ptr<TextureCubemap> TextureCubemap::Create(uint32_t face_size,
                                                            CubemapFormat format,
                                                            uint32_t mip_levels) {
        return std::make_shared<OpenGLTextureCubemap>(face_size, format, mip_levels);
    }

    namespace {

        // Reusable unit-cube geometry for the conversion pass. Built lazily on
        // first call to CreateFromEquirect, destroyed never — same lifetime as
        // the OpenGL context.
        struct EquirectConversionState {
            GLuint           cube_vao = 0;
            GLuint           cube_vbo = 0;
            std::shared_ptr<Shader> shader;
            bool             initialized = false;
        };
        EquirectConversionState g_conv;

        void EnsureConversionState() {
            if (g_conv.initialized) return;

            // Unit cube centered on origin, with positions only.
            // 36 vertices, no index buffer (matches the existing skybox layout).
            float vertices[] = {
                // +X
                 1, -1, -1,   1,  1, -1,   1,  1,  1,
                 1,  1,  1,   1, -1,  1,   1, -1, -1,
                // -X
                -1, -1,  1,  -1,  1,  1,  -1,  1, -1,
                -1,  1, -1,  -1, -1, -1,  -1, -1,  1,
                // +Y
                -1,  1, -1,   1,  1, -1,   1,  1,  1,
                 1,  1,  1,  -1,  1,  1,  -1,  1, -1,
                // -Y
                -1, -1,  1,   1, -1,  1,   1, -1, -1,
                 1, -1, -1,  -1, -1, -1,  -1, -1,  1,
                // +Z
                -1, -1,  1,   1, -1,  1,   1,  1,  1,
                 1,  1,  1,  -1,  1,  1,  -1, -1,  1,
                // -Z
                 1, -1, -1,  -1, -1, -1,  -1,  1, -1,
                -1,  1, -1,   1,  1, -1,   1, -1, -1,
            };

            glCreateVertexArrays(1, &g_conv.cube_vao);
            glCreateBuffers(1,      &g_conv.cube_vbo);
            glNamedBufferData(g_conv.cube_vbo, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glVertexArrayVertexBuffer(g_conv.cube_vao, 0, g_conv.cube_vbo, 0, 3 * sizeof(float));
            glEnableVertexArrayAttrib(g_conv.cube_vao, 0);
            glVertexArrayAttribFormat(g_conv.cube_vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
            glVertexArrayAttribBinding(g_conv.cube_vao, 0, 0);

            std::string shader_path = Project::GetEngineAssetFileSystemPath("shaders/equirect_to_cubemap").generic_string();
            g_conv.shader = AssetManager::GetShader(shader_path);

            g_conv.initialized = true;
        }

    } // anonymous namespace

    std::shared_ptr<TextureCubemap> TextureCubemap::CreateFromEquirect(
        const std::shared_ptr<Texture2D>& equirect, uint32_t face_size) {

        if (!equirect) {
            LOOM_CORE_ERROR("CreateFromEquirect: null source texture");
            return nullptr;
        }

        EnsureConversionState();

        auto cubemap = Create(face_size, CubemapFormat::RGB16F, 1);
        uint32_t cube_tex = cubemap->GetRendererID();

        // Six face view matrices. lookAt directions hit each cube face from origin.
        // Up vectors are flipped Y for the cubemap face convention (OpenGL cubemap
        // coordinates are left-handed in face layout).
        glm::mat4 proj  = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 views[6] = {
            glm::lookAt(glm::vec3(0), glm::vec3( 1,  0,  0), glm::vec3(0, -1,  0)), // +X
            glm::lookAt(glm::vec3(0), glm::vec3(-1,  0,  0), glm::vec3(0, -1,  0)), // -X
            glm::lookAt(glm::vec3(0), glm::vec3( 0,  1,  0), glm::vec3(0,  0,  1)), // +Y
            glm::lookAt(glm::vec3(0), glm::vec3( 0, -1,  0), glm::vec3(0,  0, -1)), // -Y
            glm::lookAt(glm::vec3(0), glm::vec3( 0,  0,  1), glm::vec3(0, -1,  0)), // +Z
            glm::lookAt(glm::vec3(0), glm::vec3( 0,  0, -1), glm::vec3(0, -1,  0)), // -Z
        };

        // Snapshot prior GL state so this pass doesn't disturb the caller.
        GLint prev_fbo = 0, prev_viewport[4] = { 0, 0, 0, 0 };
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        glGetIntegerv(GL_VIEWPORT, prev_viewport);
        GLboolean prev_depth   = glIsEnabled(GL_DEPTH_TEST);
        GLboolean prev_cull    = glIsEnabled(GL_CULL_FACE);

        // Conversion pass has no depth attachment and renders the cube from
        // inside; both tests would only get in the way.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        GLuint capture_fbo = 0;
        glCreateFramebuffers(1, &capture_fbo);
        glViewport(0, 0, face_size, face_size);

        g_conv.shader->Bind();
        g_conv.shader->UploadUniformInt("uEquirect", 0);
        g_conv.shader->UploadUniformMat4("uProjection", proj);
        equirect->Bind(0);

        glBindVertexArray(g_conv.cube_vao);
        glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo);

        for (int face = 0; face < 6; ++face) {
            g_conv.shader->UploadUniformMat4("uView", views[face]);
            glNamedFramebufferTextureLayer(capture_fbo, GL_COLOR_ATTACHMENT0, cube_tex, 0, face);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Restore caller's GL state.
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
        glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
        if (prev_depth) glEnable(GL_DEPTH_TEST);
        if (prev_cull)  glEnable(GL_CULL_FACE);
        glDeleteFramebuffers(1, &capture_fbo);

        return cubemap;
    }

} // namespace Loom
