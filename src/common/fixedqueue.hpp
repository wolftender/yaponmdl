#include <algorithm>
#include <array>
#include <optional>

namespace util {

template <typename T, size_t kNumElements> class FixedSizeQueue final {
public:
    enum class Result {
        eFailure,
        eSuccess,
    };

    FixedSizeQueue() : fill_{0} {}
    ~FixedSizeQueue() noexcept = default;

    auto GetSize() const -> uint32_t { return kNumElements; }
    auto GetFill() const -> uint32_t { return fill_; }
    auto Empty() const -> bool { return fill_ == 0; }
    auto Full() const -> bool { return fill_ >= kNumElements; }

    auto Peek() -> const T * {
        if (0 == fill_) {
            return nullptr;
        }

        return &buffer_[fill_ - 1];
    }

    auto Push(T &&t) -> Result {
        if (kNumElements == fill_) {
            return Result::eFailure;
        }

        buffer_[fill_++] = std::move(t);
        return Result::eSuccess;
    }

    auto Pop() -> std::optional<T> {
        if (fill_ == 0) {
            return std::nullopt;
        }

        return buffer_[--fill_];
    }

    auto Clear() -> void { fill_ = 0; }

    auto operator[](const size_t index) const -> const std::optional<T> & { return buffer_[index]; }

    template <std::invocable<const T &, const T &> F>
        requires std::convertible_to<std::invoke_result_t<F, const T &, const T &>, bool>
    auto Sort(F order) {
        if (GetFill() < 2) {
            return;
        }

        std::sort(
            buffer_.begin(), buffer_.begin() + GetFill(), [&](const auto &a, const auto &b) { return order(a, b); });
    }

private:
    std::array<std::optional<T>, kNumElements> buffer_;
    uint32_t fill_;
};

} // namespace util
