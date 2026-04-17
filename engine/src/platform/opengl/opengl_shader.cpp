#include "platform/opengl/opengl_shader.h"
#include "loom/core/log.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <vector>

namespace Loom {

    OpenGLShader::OpenGLShader(const std::string& vertex_src, const std::string& fragment_src) {
        Compile(vertex_src, fragment_src);
    }

    OpenGLShader::OpenGLShader(const std::string& filepath) {
        std::string vertex_path = filepath + ".vert";
        std::string fragment_path = filepath + ".frag";

        std::string vertex_src = ReadFile(vertex_path);
        std::string fragment_src = ReadFile(fragment_path);

        if (vertex_src.empty() || fragment_src.empty()) {
            LOOM_CORE_FATAL("Failed to load shaders from '{0}'", filepath);
            return;
        }

        Compile(vertex_src, fragment_src);
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

    void OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix) {
        GLint location = glGetUniformLocation(mRendererID, name.c_str());
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }

    void OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& values) {
        GLint location = glGetUniformLocation(mRendererID, name.c_str());
        glUniform4f(location, values.x, values.y, values.z, values.w);
    }

    void OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& values) {
        GLint location = glGetUniformLocation(mRendererID, name.c_str());
        glUniform3f(location, values.x, values.y, values.z);
    }

    void OpenGLShader::UploadUniformInt(const std::string& name, int value) {
        GLint location = glGetUniformLocation(mRendererID, name.c_str());
        glUniform1i(location, value);
    }

    std::string OpenGLShader::ReadFile(const std::string& filepath) {
        std::string result;

        std::ifstream in(filepath, std::ios::in | std::ios::binary);

        if (in) {
            in.seekg(0, std::ios::end);
            size_t size = in.tellg();

            if (size != -1) {
                result.resize(size);
                in.seekg(0, std::ios::beg);
                in.read(&result[0], size);
            } else {
                LOOM_CORE_ERROR("Could not found from file '{0}'", filepath);
            }
        } else {
            LOOM_CORE_ERROR("Could not open file '{0}'", filepath);
        }

        return result;
    }

    void OpenGLShader::Compile(const std::string& vertex_src, const std::string& fragment_src) {
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

} // namespace Loom