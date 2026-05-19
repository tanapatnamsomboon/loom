#include "platform/opengl/opengl_cubemap.h"
#include "loom/asset/asset_manager.h"
#include "loom/core/log.h"
#include "loom/project/project.h"
#include "loom/renderer/shader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

namespace Loom {

    OpenGLTextureCubemap::OpenGLTextureCubemap(uint32_t face_size, CubemapFormat format, uint32_t mip_levels)
        : mFaceSize(face_size), mMipLevels(mip_levels), mFormat(format) {
        // Non-DSA cubemap creation. DSA (glCreateTextures + glTextureStorage2D
        // for GL_TEXTURE_CUBE_MAP) miscompiles storage allocation on some
        // Intel / AMD drivers — silently producing tiny faces. glGenTextures
        // + per-face glTexImage2D is the universally-supported path.
        glGenTextures(1, &mRendererID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, mRendererID);

        GLenum internal_fmt = (format == CubemapFormat::RGB16F) ? GL_RGB16F : GL_RGBA8;
        GLenum data_fmt     = (format == CubemapFormat::RGB16F) ? GL_RGB    : GL_RGBA;
        GLenum data_type    = (format == CubemapFormat::RGB16F) ? GL_FLOAT  : GL_UNSIGNED_BYTE;

        for (uint32_t mip = 0; mip < mip_levels; ++mip) {
            uint32_t mip_size = std::max(1u, face_size >> mip);
            for (int face = 0; face < 6; ++face) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                             (GLint)mip, internal_fmt, mip_size, mip_size, 0,
                             data_fmt, data_type, nullptr);
            }
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                        mip_levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Anisotropic — helps the env / skybox cubemap at oblique view angles.
        // No-op for the 32–64² irradiance map (low-freq content) but cheap.
        float max_anisotropy = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotropy);
        glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_ANISOTROPY, max_anisotropy);

        LOOM_CORE_TRACE("OpenGLTextureCubemap created: {}x{} face, {} mip(s), format={}",
                        face_size, face_size, mip_levels,
                        format == CubemapFormat::RGB16F ? "RGB16F" : "RGBA8");
    }

    OpenGLTextureCubemap::~OpenGLTextureCubemap() {
        glDeleteTextures(1, &mRendererID);
    }

    void OpenGLTextureCubemap::Bind(uint32_t slot) const {
        glBindTextureUnit(slot, mRendererID);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Shared cube-face capture machinery
    //
    // Both equirect→cubemap and irradiance-convolution passes share the same
    // workflow: bind a unit-cube VAO, point an FBO at each of the 6 cubemap
    // faces in turn, set a per-face view matrix, render. The differences are
    // (a) which shader runs and (b) what source texture is bound. Anything
    // else is pure scaffolding and lives in `RenderToCubemapFaces` below.
    // ────────────────────────────────────────────────────────────────────────
    namespace {

        struct CubeCaptureState {
            GLuint                  cube_vao    = 0;
            GLuint                  cube_vbo    = 0;
            GLuint                  capture_fbo = 0;   // persistent — reused across passes
            std::shared_ptr<Shader> equirect_shader;
            std::shared_ptr<Shader> irradiance_shader;
            std::shared_ptr<Shader> prefilter_shader;
            bool                    initialized = false;
        };
        CubeCaptureState g_conv;

        glm::mat4 CubeFaceView(int face) {
            // Standard cube-map face views. Camera at origin, 90° FOV (set by
            // the caller), Y axis inverted on the +/-X/Z faces because the cube
            // map convention has +Y down within each face.
            static const glm::mat4 views[6] = {
                glm::lookAt(glm::vec3(0), glm::vec3( 1,  0,  0), glm::vec3(0, -1,  0)), // +X
                glm::lookAt(glm::vec3(0), glm::vec3(-1,  0,  0), glm::vec3(0, -1,  0)), // -X
                glm::lookAt(glm::vec3(0), glm::vec3( 0,  1,  0), glm::vec3(0,  0,  1)), // +Y
                glm::lookAt(glm::vec3(0), glm::vec3( 0, -1,  0), glm::vec3(0,  0, -1)), // -Y
                glm::lookAt(glm::vec3(0), glm::vec3( 0,  0,  1), glm::vec3(0, -1,  0)), // +Z
                glm::lookAt(glm::vec3(0), glm::vec3( 0,  0, -1), glm::vec3(0, -1,  0)), // -Z
            };
            return views[face];
        }

        void EnsureConversionState() {
            if (g_conv.initialized) return;

            // Unit cube centered on origin, position-only. 36 vertices, no
            // index buffer (matches the existing skybox layout exactly).
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

            glCreateFramebuffers(1, &g_conv.capture_fbo);

            std::string equirect_path   = Project::GetEngineAssetFileSystemPath("shaders/equirect_to_cubemap").generic_string();
            std::string irradiance_path = Project::GetEngineAssetFileSystemPath("shaders/irradiance_convolution").generic_string();
            std::string prefilter_path  = Project::GetEngineAssetFileSystemPath("shaders/prefilter_convolution").generic_string();
            g_conv.equirect_shader   = AssetManager::GetShader(equirect_path);
            g_conv.irradiance_shader = AssetManager::GetShader(irradiance_path);
            g_conv.prefilter_shader  = AssetManager::GetShader(prefilter_path);

            g_conv.initialized = true;
            LOOM_CORE_TRACE("IBL: cube-capture state initialized (VAO + persistent FBO + shaders loaded)");
        }

        // Renders the unit cube into all 6 faces of `target_cubemap` at the
        // given mip level. The caller is expected to have already bound the
        // shader, set source textures, and uploaded uEquirect / uEnvironment
        // uniforms. This helper handles uProjection + per-face uView upload,
        // FBO attach, GL state save/restore, and the 6 draws.
        void RenderToCubemapFaces(TextureCubemap& target_cubemap,
                                  Shader&         shader,
                                  uint32_t        mip_level = 0) {
            EnsureConversionState();

            const uint32_t face_size = std::max(1u, target_cubemap.GetFaceSize() >> mip_level);
            const GLuint   cube_tex  = target_cubemap.GetRendererID();

            // Snapshot caller state.
            GLint     prev_fbo = 0, prev_viewport[4] = { 0, 0, 0, 0 };
            GLboolean prev_depth, prev_cull;
            GLint     prev_cull_mode = GL_BACK;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
            glGetIntegerv(GL_VIEWPORT,            prev_viewport);
            prev_depth = glIsEnabled(GL_DEPTH_TEST);
            prev_cull  = glIsEnabled(GL_CULL_FACE);
            glGetIntegerv(GL_CULL_FACE_MODE, &prev_cull_mode);

            // Pass setup. Camera at origin looking outward; 90° FOV means each
            // cube face exactly fills the framebuffer when its 6 vertices are
            // rendered alone. No depth test needed (no overlap possible), no
            // face culling needed (single planar quad, two triangles, both
            // facing the camera from inside the cube).
            //
            // Crucial: draw ONLY the 6 vertices for the current face, NOT all
            // 36. If we drew all 36, the other 4 visible faces' triangles
            // would also project into this view's framebuffer (their corners
            // sit inside the 90° frustum) and — with no depth test — the
            // last-drawn face would overwrite the intended face's edges.
            // That's the bug that turns the irradiance cubemap into a
            // patchwork of face-content cross-contamination, visible as hard
            // colored regions on a sphere wrapping the cubemap.
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glViewport(0, 0, face_size, face_size);

            glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
            shader.UploadUniformMat4("uProjection", proj);

            glBindVertexArray(g_conv.cube_vao);
            glBindFramebuffer(GL_FRAMEBUFFER, g_conv.capture_fbo);

            for (int face = 0; face < 6; ++face) {
                shader.UploadUniformMat4("uView", CubeFaceView(face));
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                                       cube_tex, (GLint)mip_level);
                glClear(GL_COLOR_BUFFER_BIT);
                glDrawArrays(GL_TRIANGLES, 6 * face, 6);
            }

            // Restore caller state. Order matters — restore depth/cull before
            // FBO so the next user's first draw doesn't inherit our scratch.
            if (prev_depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (prev_cull)  glEnable(GL_CULL_FACE);   else glDisable(GL_CULL_FACE);
            glCullFace((GLenum)prev_cull_mode);
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
            glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
        }

    } // anonymous namespace

    // ────────────────────────────────────────────────────────────────────────
    // Static factories on TextureCubemap (OpenGL impl, lives here)
    // ────────────────────────────────────────────────────────────────────────

    std::shared_ptr<TextureCubemap> TextureCubemap::Create(uint32_t face_size,
                                                            CubemapFormat format,
                                                            uint32_t mip_levels) {
        return std::make_shared<OpenGLTextureCubemap>(face_size, format, mip_levels);
    }

    namespace {
        // log2(N) + 1, i.e. full mip chain length for an NxN face.
        uint32_t MipCountForFaceSize(uint32_t face_size) {
            return (uint32_t)std::floor(std::log2((double)face_size)) + 1u;
        }
    }

    std::shared_ptr<TextureCubemap> TextureCubemap::CreateFromEquirect(
        const std::shared_ptr<Texture2D>& equirect, uint32_t face_size) {

        if (!equirect) {
            LOOM_CORE_ERROR("CreateFromEquirect: null source texture");
            return nullptr;
        }
        EnsureConversionState();

        // Full mip chain — skybox sampling on the env cubemap is a minification
        // problem at oblique angles, so we need lower mips for trilinear
        // filtering to anti-alias. B.3 (specular prefilter) will also store
        // roughness-convolved variants in these mip slots.
        const uint32_t mip_levels = MipCountForFaceSize(face_size);
        auto cubemap = Create(face_size, CubemapFormat::RGB16F, mip_levels);

        g_conv.equirect_shader->Bind();
        g_conv.equirect_shader->UploadUniformInt("uEquirect", 0);
        equirect->Bind(0);

        RenderToCubemapFaces(*cubemap, *g_conv.equirect_shader);

        // Fill mip 1..N-1 by averaging from mip 0 (bilinear box filter).
        // For the skybox path this is enough; the specular prefilter slice
        // will overwrite these mips with proper roughness-convolved variants.
        glGenerateTextureMipmap(cubemap->GetRendererID());

        LOOM_CORE_TRACE("IBL: built env cubemap {}x{} ({} mips) from {}x{} HDR equirect",
                        face_size, face_size, mip_levels,
                        equirect->GetWidth(), equirect->GetHeight());
        return cubemap;
    }

    std::shared_ptr<TextureCubemap> TextureCubemap::CreateIrradiance(
        const std::shared_ptr<TextureCubemap>& env_cubemap, uint32_t face_size) {

        if (!env_cubemap) {
            LOOM_CORE_ERROR("CreateIrradiance: null source cubemap");
            return nullptr;
        }
        EnsureConversionState();

        // Full mip chain so trilinear filtering can anti-alias the irradiance
        // sample on curved surfaces. Without mips, the bilinear samples on a
        // sphere alias against the cubemap texel grid → visible moiré bands
        // (the artifact we're fixing here).
        const uint32_t mip_levels = MipCountForFaceSize(face_size);
        auto cubemap = Create(face_size, CubemapFormat::RGB16F, mip_levels);

        g_conv.irradiance_shader->Bind();
        g_conv.irradiance_shader->UploadUniformInt   ("uEnvironment",    0);
        // Tell the convolution shader the source resolution so it can pick a
        // mip LOD matching our sample density (Karis pre-filtering trick).
        g_conv.irradiance_shader->UploadUniformFloat ("uSourceFaceSize",
                                                       (float)env_cubemap->GetFaceSize());
        env_cubemap->Bind(0);

        RenderToCubemapFaces(*cubemap, *g_conv.irradiance_shader);

        // Bilinear-averaged mip chain. Diffuse irradiance is already low-pass
        // by construction, so a box-filter mip is an accurate enough lower-
        // frequency representation for minification.
        glGenerateTextureMipmap(cubemap->GetRendererID());

        LOOM_CORE_TRACE("IBL: built irradiance cubemap {}x{} ({} mips) from {}x{} env",
                        face_size, face_size, mip_levels,
                        env_cubemap->GetFaceSize(), env_cubemap->GetFaceSize());
        return cubemap;
    }

    std::shared_ptr<TextureCubemap> TextureCubemap::CreatePrefiltered(
        const std::shared_ptr<TextureCubemap>& env_cubemap, uint32_t face_size) {

        if (!env_cubemap) {
            LOOM_CORE_ERROR("CreatePrefiltered: null source cubemap");
            return nullptr;
        }
        EnsureConversionState();

        // Full mip chain — each mip stores the env convolved at a different
        // roughness. mip 0 = mirror, deepest mip = fully rough. The shader
        // samples this with `textureLod(prefilter, R, roughness * maxLOD)`.
        const uint32_t mip_levels = MipCountForFaceSize(face_size);
        auto cubemap = Create(face_size, CubemapFormat::RGB16F, mip_levels);

        g_conv.prefilter_shader->Bind();
        g_conv.prefilter_shader->UploadUniformInt   ("uEnvironment",    0);
        g_conv.prefilter_shader->UploadUniformFloat ("uSourceFaceSize",
                                                      (float)env_cubemap->GetFaceSize());
        env_cubemap->Bind(0);

        for (uint32_t mip = 0; mip < mip_levels; ++mip) {
            // Roughness sweep: mip 0 → 0.0 (perfect mirror), last mip → 1.0.
            // The single-mip degenerate case (face_size = 1) maps to roughness
            // 0 — harmless since there's no meaningful convolution to do.
            float roughness = (mip_levels > 1)
                ? float(mip) / float(mip_levels - 1)
                : 0.0f;
            g_conv.prefilter_shader->UploadUniformFloat("uRoughness", roughness);
            RenderToCubemapFaces(*cubemap, *g_conv.prefilter_shader, mip);
        }

        LOOM_CORE_TRACE("IBL: built prefilter cubemap {}x{} ({} mips) from {}x{} env",
                        face_size, face_size, mip_levels,
                        env_cubemap->GetFaceSize(), env_cubemap->GetFaceSize());
        return cubemap;
    }

} // namespace Loom
