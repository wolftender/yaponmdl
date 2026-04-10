#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "common/common.hpp"

#include "graphics/gl.hpp"
#include "graphics/buffer.hpp"

namespace gl {

class ShaderProgram final {
public:
    class ShaderCompileError final : public std::exception {
    public:
        ShaderCompileError(const std::string &log) : log_{log} {}
        ~ShaderCompileError() = default;

        auto what() const noexcept -> const char * { return log_.c_str(); }

    private:
        std::string log_;
    };

    class ShaderLinkerError final : public std::exception {
    public:
        ShaderLinkerError(const std::string &log) : log_{log} {}
        ~ShaderLinkerError() = default;

        auto what() const noexcept -> const char * { return log_.c_str(); }

    private:
        std::string log_;
    };

    struct UniformBinding {
        std::string name;
        GLint location;
        GLint size;
        GLenum type;

        UniformBinding(const std::string &name, GLint location, GLint size, GLenum type)
            : name{name}, location{location}, size{size}, type{type} {}
    };

    struct UniformBlockBinding {
        std::string name;
        GLint data_size;
        GLuint index;

        UniformBlockBinding(const std::string &name, GLint data_size, GLuint index)
            : name{name}, data_size{data_size}, index{index} {}
    };

    class Context {
    public:
        Context(const ShaderProgram &shader) : shader_{shader} { glUseProgram(shader_.handle_); }
        ~Context() noexcept { glUseProgram(0); }

        Context(const Context &) = delete;
        auto operator=(const Context &) = delete;

        template <StringValue S> auto SetFlag(const S &name, const bool &value) const -> void {
            WithUniform(name, GL_BOOL, [&](const auto &u) { glUniform1i(u.location, value); });
        }

        template <StringValue S> auto SetUniform(const S &name, const int32_t &value) const -> void {
            WithUniform(name, GL_INT, [&](const auto &u) { glUniform1i(u.location, value); });
        }

        template <StringValue S> auto SetUniform(const S &name, const float &value) const -> void {
            WithUniform(name, GL_FLOAT, [&](const auto &u) { glUniform1f(u.location, value); });
        }

        template <StringValue S> auto SetUniform(const S &name, const glm::fvec2 &value) const -> void {
            WithUniform(
                name, GL_FLOAT_VEC2, [&](const auto &u) { glUniform2fv(u.location, 1, glm::value_ptr(value)); });
        }

        template <StringValue S> auto SetUniform(const S &name, const glm::fvec3 &value) const -> void {
            WithUniform(
                name, GL_FLOAT_VEC3, [&](const auto &u) { glUniform3fv(u.location, 1, glm::value_ptr(value)); });
        }

        template <StringValue S> auto SetUniform(const S &name, const glm::fmat2 &value) const -> void {
            WithUniform(name, GL_FLOAT_MAT2, [&](const auto &u) {
                glUniformMatrix2fv(u.location, 1, GL_FALSE, glm::value_ptr(value));
            });
        }

        template <StringValue S> auto SetUniform(const S &name, const glm::fmat3 &value) const -> void {
            WithUniform(name, GL_FLOAT_MAT3, [&](const auto &u) {
                glUniformMatrix3fv(u.location, 1, GL_FALSE, glm::value_ptr(value));
            });
        }

        template <StringValue S> auto SetUniform(const S &name, const glm::fmat4 &value) const -> void {
            WithUniform(name, GL_FLOAT_MAT4, [&](const auto &u) {
                glUniformMatrix4fv(u.location, 1, GL_FALSE, glm::value_ptr(value));
            });
        }

        template <StringValue S> auto SetSampler(const S &name, const uint32_t slot) const -> void {
            WithUniform(name, GL_SAMPLER_2D, [&](const auto &u) { glUniform1i(u.location, static_cast<GLint>(slot)); });
        }

        template <StringValue S, typename T>
        auto SetBuffer(const S &name, const UniformBuffer<T> &buffer) const -> void {
            shader_.executor_.RunOnContext([&](const auto &&) {
                auto it = shader_.uniform_blocks_.find(name);
                if (shader_.uniform_blocks_.end() == it) {
                    return;
                }

                GL_CHECK(glUniformBlockBinding(shader_.handle_, it->second.index, buffer.get_base()));
            });
        }

    private:
        template <std::invocable<const UniformBinding &> F, StringValue S>
        inline auto WithUniform(const S &name, GLenum type, const F &&f) const -> void {
            shader_.executor_.RunOnContext([&](const auto &) {
                auto it = shader_.uniforms_.find(name);
                if (shader_.uniforms_.end() == it) {
                    return;
                }

                if (it->second.type != type) {
                    return;
                }

                f(it->second);
            });
        }

        const ShaderProgram &shader_;
    };

    template <std::invocable<const Context &> F> inline auto Use(const F &&f) -> void {
        executor_.RunOnContext([&](const auto &) {
            Context context{*this};
            f(context);
        });
    }

    ShaderProgram(GLContext::Executor executor, const std::string_view &vs_source, const std::string_view &fs_source);
    ShaderProgram(const ShaderProgram &) = delete;
    auto operator=(const ShaderProgram &) -> ShaderProgram & = delete;
    ShaderProgram(ShaderProgram &&) = default;
    auto operator=(ShaderProgram &&) -> ShaderProgram & = default;

    auto GetExecutor() const -> const GLContext::Executor & { return executor_; }

private:
    auto MapUniforms() -> void;
    auto MapUniformBlocks() -> void;

    static auto CompileShader(GLContext::Executor executor, const GLenum type, const std::string_view &source)
        -> ShaderHandle;

    struct StringHash {
        using is_transparent = void;
        [[nodiscard]] auto operator()(const char *s) const -> size_t { return std::hash<std::string_view>{}(s); }

        [[nodiscard]] auto operator()(std::string_view s) const -> size_t { return std::hash<std::string_view>{}(s); }

        [[nodiscard]] auto operator()(const std::string &s) const -> size_t { return std::hash<std::string>{}(s); }
    };

    GLContext::Executor executor_;
    std::unordered_map<std::string, UniformBlockBinding, StringHash, std::equal_to<>> uniform_blocks_;
    std::unordered_map<std::string, UniformBinding, StringHash, std::equal_to<>> uniforms_;
    ShaderProgramHandle handle_;
};

} // namespace gl
