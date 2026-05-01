#pragma once
#include <optional>

#include "byteutils.hpp"

#include <glm/glm.hpp>

namespace util::bytes {

class BinaryReader final {
public:
    enum class Result {
        eFailure = 0,
        eSuccess = 1,
    };

    explicit BinaryReader(std::span<const u8> input_range) : data_span_{std::move(input_range)}, ptr_{0ull} {}
    ~BinaryReader() = default;

    BinaryReader(const BinaryReader &) = default;
    auto operator=(const BinaryReader &) -> BinaryReader & = default;

    BinaryReader(BinaryReader &&) noexcept = default;
    auto operator=(BinaryReader &&) noexcept -> BinaryReader & = default;

    auto Position() const -> u64 { return ptr_; }
    auto Remaining() const -> u64;
    auto Seek(u64 location) -> Result;
    auto ReadBuffer(u64 num_bytes) -> std::optional<std::span<const u8>>;

    template <IsPrimitiveType T> auto ReadAligned() -> std::optional<T> {
        constexpr auto type_size = sizeof(T);
        const auto end_ptr = data_span_.size();

        const auto align_ptr = (ptr_ + (type_size - 1)) & ~(type_size - 1);
        if (align_ptr + type_size > end_ptr) {
            return std::nullopt;
        }

        T result = *(reinterpret_cast<const T *>(data_span_.data() + align_ptr));

        ptr_ = align_ptr + type_size;
        return result;
    }

    template <IsPrimitiveType T> auto Read() -> std::optional<T> {
        constexpr auto type_size = sizeof(T);
        const auto end_ptr = data_span_.size();

        if (ptr_ + type_size > end_ptr) {
            return std::nullopt;
        }

        // little endian data read
        T result = *(reinterpret_cast<const T *>(data_span_.data() + ptr_));

        ptr_ = ptr_ + type_size;
        return result;
    }

    template <typename T = f32> auto ReadVec2() -> std::optional<glm::vec<2, T>> {
        const auto x = Read<T>();
        const auto y = Read<T>();

        if (!x.has_value() || !y.has_value()) {
            return std::nullopt;
        }

        return glm::vec<2, T>{*x, *y};
    }

    template <typename T = f32> auto ReadVec3() -> std::optional<glm::vec<3, T>> {
        const auto x = Read<T>();
        const auto y = Read<T>();
        const auto z = Read<T>();

        if (!x.has_value() || !y.has_value() || !z.has_value()) {
            return std::nullopt;
        }

        return glm::vec<3, T>{*x, *y, *z};
    }

    template <typename T = f32> auto ReadVec4() -> std::optional<glm::vec<4, T>> {
        const auto x = Read<T>();
        const auto y = Read<T>();
        const auto z = Read<T>();
        const auto w = Read<T>();

        if (!x.has_value() || !y.has_value() || !z.has_value() || !w.has_value()) {
            return std::nullopt;
        }

        return glm::vec<4, T>{*x, *y, *z, *w};
    }

    template <typename T = f32> auto ReadQuat() -> std::optional<glm::qua<T>> {
        const auto x = Read<T>();
        const auto y = Read<T>();
        const auto z = Read<T>();
        const auto w = Read<T>();

        if (!x.has_value() || !y.has_value() || !z.has_value() || !w.has_value()) {
            return std::nullopt;
        }

        return glm::qua<T>{*w, *x, *y, *z}; // this constructor has order WXYZ
    }

private:
    std::span<const u8> data_span_;
    u64 ptr_;
};

} // namespace util::bytes
