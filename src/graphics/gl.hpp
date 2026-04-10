#pragma once
#include <span>
#include <cassert>
#include <concepts>
#include <memory>
#include <thread>
#include <string_view>

#include <fmt/format.h>
#include <glad/glad.h>

namespace gl {

template <typename T>
concept HandleDeleter = std::invocable<T, GLuint>;

enum class ShaderPrimitiveType : GLenum {
    eInt = GL_INT,
    eIntVec2 = GL_INT_VEC2,
    eIntVec3 = GL_INT_VEC3,
    eIntVec4 = GL_INT_VEC4,
    eUInt = GL_UNSIGNED_INT,
    eUIntVec2 = GL_UNSIGNED_INT_VEC2,
    eUIntVec3 = GL_UNSIGNED_INT_VEC3,
    eUIntVec4 = GL_UNSIGNED_INT_VEC4,
    eFloat = GL_FLOAT,
    eFloatVec2 = GL_FLOAT_VEC2,
    eFloatVec3 = GL_FLOAT_VEC3,
    eFloatVec4 = GL_FLOAT_VEC4,
    eFloatMat2 = GL_FLOAT_MAT2,
    eFloatMat3 = GL_FLOAT_MAT3,
    eFloatMat4 = GL_FLOAT_MAT4,
    eSampler1D = GL_SAMPLER_1D,
    eSampler2D = GL_SAMPLER_2D,
    eSampler3D = GL_SAMPLER_3D
};

enum class ShaderAttribType : GLenum {
    eByte = GL_BYTE,
    eUnsignedByte = GL_UNSIGNED_BYTE,
    eShort = GL_SHORT,
    eUnsignedShort = GL_UNSIGNED_SHORT,
    eInt = GL_INT,
    eUnsignedInt = GL_UNSIGNED_INT,
    eHalfFloat = GL_HALF_FLOAT,
    eFloat = GL_FLOAT,
    eDouble = GL_DOUBLE
};

struct LayoutElement {
    uint32_t index;
    uint32_t size;
    ShaderAttribType type;
    uint32_t stride;
    uint32_t offset;

    constexpr LayoutElement(uint32_t _index, uint32_t _size, ShaderAttribType _type, uint32_t _stride, uint32_t _offset)
        : index{_index}, size{_size}, type{_type}, stride{_stride}, offset{_offset} {}
};

template <typename T>
concept VertexType = requires() {
    { T::GetLayout() } -> std::convertible_to<std::span<const LayoutElement>>;
};

class GLContext {
private:
    friend class Controller;
    class Controller final {
    public:
        Controller(GLContext *context) : context_{context} {}
        Controller(const Controller &) = delete;
        auto operator=(const Controller &) -> Controller & = delete;

        inline auto StartContext() const -> void { context_->StartContext(); }
        inline auto EndContext() const -> void { context_->EndContext(); }

        auto GetContext() const -> GLContext & { return *context_; }

    private:
        mutable GLContext *context_;
    };

    std::shared_ptr<Controller> controller_;

protected:
    virtual auto StartContext() const -> void = 0;
    virtual auto EndContext() const -> void = 0;
    virtual auto LogMessage(std::string_view message) const -> void = 0;

public:
    class Executor final {
    public:
        friend class GLContext;

        Executor(const Executor &) = default;
        auto operator=(const Executor &) -> Executor & = default;

        Executor(Executor &&) = default;
        auto operator=(Executor &&) -> Executor & = default;

        template <std::invocable<GLContext &> F> auto RunOnContext(F task) const -> void {
            // not allowed from another thread
            if (std::this_thread::get_id() != owner_thread_) {
                return;
            }

            bool has_to_end_context = false;

            if (!s_controller) {
                s_controller = controller_.lock();

                // alredy destroyed context
                if (!s_controller) {
                    return;
                }

                has_to_end_context = true;
                s_controller->StartContext();
            }

            task(s_controller->GetContext());

            if (has_to_end_context) {
                s_controller->EndContext();
                s_controller = nullptr;
            }
        }

        static auto ThreadHasContext() -> bool { return s_controller != nullptr; }

        static auto LogMessage(std::string_view message) {
            if (ThreadHasContext()) {
                s_controller->GetContext().LogMessage(message);
            }
        }

    private:
        Executor(std::thread::id owner_thread, std::weak_ptr<Controller> controller)
            : owner_thread_{owner_thread}, controller_{std::move(controller)} {}

        std::thread::id owner_thread_;
        std::weak_ptr<Controller> controller_;

        static thread_local std::shared_ptr<Controller> s_controller;
    };

    virtual ~GLContext() = default;

    GLContext() : controller_{std::make_shared<Controller>(this)} {}
    GLContext(const GLContext &) = delete;
    auto operator=(const GLContext &) -> GLContext & = delete;

    virtual auto IsReady() const -> bool = 0;
    virtual auto GetOwnerThread() const -> std::thread::id = 0;
    virtual auto IsContextThread() const -> bool = 0;
    virtual auto HasExtension(std::string_view extension) const -> bool = 0;
    virtual auto GetVersion() const -> std::pair<uint32_t, uint32_t> = 0;

    auto GetExecutor() const -> Executor { return Executor{GetOwnerThread(), controller_}; }
};

// use this macro in every function that asserts opengl is used but does not explicitly run on an executor
#define GL_IMPLEMENTATION_INTERNAL                                                                                     \
    assert(gl::GLContext::Executor::ThreadHasContext() && "this thread does not have an active gl context");

#ifdef NDEBUG
#define GL_CHECK(...) (__VA_ARGS__)
#else
#define GL_CHECK(...)                                                                                                  \
    [](auto &&block) {                                                                                                 \
        GL_IMPLEMENTATION_INTERNAL;                                                                                    \
        auto get_error = []() -> bool {                                                                                \
            const auto error_code = glGetError();                                                                      \
            switch (error_code) {                                                                                      \
            case GL_NO_ERROR:                                                                                          \
                return false;                                                                                          \
            case GL_INVALID_ENUM:                                                                                      \
                gl::GLContext::Executor::LogMessage(                                                                   \
                    fmt::format("OpenGL error {}: GL_INVALID_ENUM at {}", error_code, #__VA_ARGS__));                  \
                return true;                                                                                           \
            case GL_INVALID_VALUE:                                                                                     \
                gl::GLContext::Executor::LogMessage(                                                                   \
                    fmt::format("OpenGL error {}: GL_INVALID_VALUE at {}", error_code, #__VA_ARGS__));                 \
                return true;                                                                                           \
            case GL_INVALID_OPERATION:                                                                                 \
                gl::GLContext::Executor::LogMessage(                                                                   \
                    fmt::format("OpenGL error {}: GL_INVALID_OPERATION at {}", error_code, #__VA_ARGS__));             \
                return true;                                                                                           \
            case GL_INVALID_FRAMEBUFFER_OPERATION:                                                                     \
                gl::GLContext::Executor::LogMessage(                                                                   \
                    fmt::format("OpenGL error {}: GL_INVALID_FRAMEBUFFER_OPERATION at {}", error_code, #__VA_ARGS__)); \
                return true;                                                                                           \
            case GL_OUT_OF_MEMORY:                                                                                     \
                gl::GLContext::Executor::LogMessage(                                                                   \
                    fmt::format("OpenGL error {}: GL_OUT_OF_MEMORY at {}", error_code, #__VA_ARGS__));                 \
                return true;                                                                                           \
            default:                                                                                                   \
                gl::GLContext::Executor::LogMessage(                                                                   \
                    fmt::format("OpenGL error {}: Unknown at {}", error_code, #__VA_ARGS__));                          \
                return true;                                                                                           \
            }                                                                                                          \
        };                                                                                                             \
                                                                                                                       \
        while (get_error())                                                                                            \
            ;                                                                                                          \
                                                                                                                       \
        if constexpr (std::is_void_v<decltype(block())>) {                                                             \
            block();                                                                                                   \
            while (get_error())                                                                                        \
                ;                                                                                                      \
        } else {                                                                                                       \
            const auto ret = block();                                                                                  \
            while (get_error())                                                                                        \
                ;                                                                                                      \
            return ret;                                                                                                \
        }                                                                                                              \
    }([&]() { return __VA_ARGS__; })
#endif

template <typename Deleter> class HandleWrapper final {
public:
    explicit HandleWrapper(Deleter &&deleter) : deleter_{deleter}, handle_{0} {}
    explicit HandleWrapper(const GLContext::Executor &executor) : deleter_{executor}, handle_{0} {}

    HandleWrapper(const GLuint handle, Deleter &&deleter) : deleter_{deleter}, handle_{handle} {}
    ~HandleWrapper() {
        if (handle_ != 0) {
            deleter_(handle_);
        }
    }

    HandleWrapper(HandleWrapper<Deleter> &&w) noexcept : deleter_{std::move(w.deleter_)} {
        handle_ = w.handle_;
        w.handle_ = 0;
    }

    operator bool() const { return (handle_ != 0); }
    operator GLuint() const { return handle_; }
    auto operator=(const GLuint handle) -> HandleWrapper<Deleter> & {
        Reset(handle);
        return *this;
    }

    auto operator=(HandleWrapper<Deleter> &&w) noexcept -> HandleWrapper<Deleter> & {
        if (this != &w) {
            deleter_ = std::move(w.deleter_);
            handle_ = w.handle_;
            w.handle_ = 0;
        }

        return *this;
    }

    auto Reset(GLuint handle) {
        if (handle_ && handle != handle_) {
            Deleter deleter;
            deleter(handle_);
        }

        handle_ = handle;
    }

    HandleWrapper(const HandleWrapper<Deleter> &) = delete;
    auto operator=(const HandleWrapper<Deleter> &) = delete;

private:
    Deleter deleter_;
    GLuint handle_;
};

namespace deleters {
struct VertexArray {
private:
    GLContext::Executor executor_;

public:
    explicit VertexArray(GLContext::Executor executor) : executor_{executor} {}
    auto operator()(const GLuint &h) {
        executor_.RunOnContext([&]([[maybe_unused]] GLContext &) { GL_CHECK(glDeleteVertexArrays(1, &h)); });
    }
};

struct Buffer {
private:
    GLContext::Executor executor_;

public:
    explicit Buffer(GLContext::Executor executor) : executor_{executor} {}
    auto operator()(const GLuint &h) {
        executor_.RunOnContext([&]([[maybe_unused]] GLContext &) { GL_CHECK(glDeleteBuffers(1, &h)); });
    }
};

struct Shader {
private:
    GLContext::Executor executor_;

public:
    explicit Shader(GLContext::Executor executor) : executor_{executor} {}
    auto operator()(const GLuint &h) {
        executor_.RunOnContext([&]([[maybe_unused]] GLContext &) { GL_CHECK(glDeleteShader(h)); });
    }
};

struct ShaderProgram {
private:
    GLContext::Executor executor_;

public:
    explicit ShaderProgram(GLContext::Executor executor) : executor_{executor} {}
    auto operator()(const GLuint &h) {
        executor_.RunOnContext([&]([[maybe_unused]] GLContext &) { GL_CHECK(glDeleteProgram(h)); });
    }
};

struct Texture {
private:
    GLContext::Executor executor_;

public:
    explicit Texture(GLContext::Executor executor) : executor_{executor} {}
    auto operator()(const GLuint &h) {
        executor_.RunOnContext([&]([[maybe_unused]] GLContext &) { GL_CHECK(glDeleteTextures(1, &h)); });
    }
};

struct Framebuffer {
private:
    GLContext::Executor executor_;

public:
    explicit Framebuffer(GLContext::Executor executor) : executor_{executor} {}
    auto operator()(const GLuint &h) {
        executor_.RunOnContext([&]([[maybe_unused]] GLContext &) { GL_CHECK(glDeleteFramebuffers(1, &h)); });
    }
};

struct Renderbuffer {
private:
    GLContext::Executor executor_;

public:
    explicit Renderbuffer(GLContext::Executor executor) : executor_{executor} {}
    auto operator()(const GLuint &h) {
        executor_.RunOnContext([&]([[maybe_unused]] GLContext &) { GL_CHECK(glDeleteRenderbuffers(1, &h)); });
    }
};
} // namespace deleters

using VertexArrayHandle = HandleWrapper<deleters::VertexArray>;
using BufferHandle = HandleWrapper<deleters::Buffer>;
using ShaderHandle = HandleWrapper<deleters::Shader>;
using ShaderProgramHandle = HandleWrapper<deleters::ShaderProgram>;
using TextureHandle = HandleWrapper<deleters::Texture>;
using FramebufferHandle = HandleWrapper<deleters::Framebuffer>;
using RenderbufferHandle = HandleWrapper<deleters::Renderbuffer>;

namespace factories {

auto MakeShader(GLContext::Executor executor, const GLenum type) -> ShaderHandle;
auto MakeProgram(GLContext::Executor executor) -> ShaderProgramHandle;
auto MakeBuffer(GLContext::Executor executor) -> BufferHandle;
auto MakeArray(GLContext::Executor executor) -> VertexArrayHandle;
auto MakeTexture(GLContext::Executor executor) -> TextureHandle;
auto MakeFramebuffer(GLContext::Executor executor) -> FramebufferHandle;
auto MakeRenderbuffer(GLContext::Executor executor) -> RenderbufferHandle;

} // namespace factories

} // namespace gl