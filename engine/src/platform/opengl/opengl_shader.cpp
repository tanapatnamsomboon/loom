#include "platform/opengl/opengl_shader.h"
#include "loom/core/log.h"
#include <glad/glad.h>
#include <vector>

namespace Loom {

    OpenGLShader::OpenGLShader(const std::string& vertex_src, const std::string& fragment_src) {
        GLuint program = glCreateProgram();

        GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        const GLchar* source = (const GLchar*)vertex_src.c_str();
        glShaderSource(vertex_shader, 1, &source, nullptr);
        glCompileShader(vertex_shader);

        GLint is_compiled = 0;
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &is_compiled);
        if (is_compiled == GL_FALSE) {
            GLint max_length = 0;
            glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &max_length);

            std::vector<GLchar> info_log(max_length);
            glGetShaderInfoLog(vertex_shader, max_length, &max_length, &info_log[0]);

            glDeleteShader(vertex_shader);
            LOOM_CORE_ERROR("{0}", info_log.data());
            LOOM_CORE_FATAL("Vertex shader compilation failed!");
            return;
        }

        GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        source = (const GLchar*)fragment_src.c_str();
        glShaderSource(fragment_shader, 1, &source, nullptr);
        glCompileShader(fragment_shader);

        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &is_compiled);
        if (is_compiled == GL_FALSE) {
            GLint max_length = 0;
            glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &max_length);

            std::vector<GLchar> info_log(max_length);
            glGetShaderInfoLog(fragment_shader, max_length, &max_length, &info_log[0]);

            glDeleteShader(fragment_shader);
            glDeleteShader(vertex_shader);
            LOOM_CORE_ERROR("{0}", info_log.data());
            LOOM_CORE_FATAL("Fragment shader compilation failed!");
            return;
        }

        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);

        GLint is_linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, (int*)&is_linked);
        if (is_linked == GL_FALSE) {
            GLint max_length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &max_length);

            std::vector<GLchar> info_log(max_length);
            glGetProgramInfoLog(program, max_length, &max_length, &info_log[0]);

            glDeleteProgram(program);
            glDeleteShader(vertex_shader);
            glDeleteShader(fragment_shader);

            LOOM_CORE_ERROR("{0}", info_log.data());
            LOOM_CORE_FATAL("Shader link failed!");
            return;
        }

        glDetachShader(program, vertex_shader);
        glDetachShader(program, fragment_shader);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);

        mRendererID = program;
    }

    OpenGLShader::~OpenGLShader() {
        glDeleteProgram(mRendererID);
    }

    void OpenGLShader::Bind() const {
        glUseProgram(mRendererID);
    }

    void OpenGLShader::Unbind() const {
        glUseProgram(0);
    }

} // namespace Loom