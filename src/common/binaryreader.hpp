#pragma once
#include <bit>
#include <cstring>
#include <optional>

#include "byteutils.hpp"

#include <glm/glm.hpp>

namespace util::bytes {

class BinaryReader final {
private:
    // clang-format off
    template <std::size_t T> struct SizedUint;
    template <> struct SizedUint<1u> { using Type = uint8_t; };
    template <> struct SizedUint<2u> { using Type = uint16_t; };
    template <> struct SizedUint<4u> { using Type = uint32_t; };
    template <> struct SizedUint<8u> { using Type = uint64_t; };
    // clang-format on

public:
    enum class Result {
        eFailure = 0,
        eSuccess = 1,
    };

    enum class ByteOrder {
        eLittleEndian = 0,
        eBigEndian = 1,
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

    template <ByteOrder Order, IsPrimitiveType T> auto ReadAligned() -> std::optional<T> {
        constexpr auto type_size = sizeof(T);
        const auto end_ptr = data_span_.size();

        using sized_uint_t = typename SizedUint<type_size>::Type;

        const auto align_ptr = (ptr_ + (type_size - 1)) & ~(type_size - 1);
        if (align_ptr + type_size > end_ptr) {
            return std::nullopt;
        }

        T result;

        if constexpr (Order == ByteOrder::eBigEndian) {
            sized_uint_t uresult;
            std::memcpy(&uresult, data_span_.data() + align_ptr, type_size);
            uresult = std::byteswap(uresult);
            std::memcpy(&result, &uresult, type_size);
        } else if constexpr (Order == ByteOrder::eLittleEndian) {
            std::memcpy(&result, data_span_.data() + align_ptr, type_size);
        } else {
            static_assert(false, "invalid byte order");
        }

        ptr_ = align_ptr + type_size;
        return result;
    }

    template <ByteOrder Order, IsPrimitiveType T> auto Read() -> std::optional<T> {
        constexpr auto type_size = sizeof(T);
        const auto end_ptr = data_span_.size();

        using sized_uint_t = typename SizedUint<type_size>::Type;

        if (ptr_ + type_size > end_ptr) {
            return std::nullopt;
        }

        T result;
        if constexpr (Order == ByteOrder::eBigEndian) {
            sized_uint_t uresult;
            std::memcpy(&uresult, data_span_.data() + ptr_, type_size);
            uresult = std::byteswap(uresult);
            std::memcpy(&result, &uresult, type_size);
        } else if constexpr (Order == ByteOrder::eLittleEndian) {
            std::memcpy(&result, data_span_.data() + ptr_, type_size);
        } else {
            static_assert(false, "invalid byte order");
        }

        ptr_ = ptr_ + type_size;
        return result;
    }

    template <ByteOrder Order, typename T = f32> auto ReadVec2() -> std::optional<glm::vec<2, T>> {
        const auto x = Read<Order, T>();
        const auto y = Read<Order, T>();

        if (!x.has_value() || !y.has_value()) {
            return std::nullopt;
        }

        return glm::vec<2, T>{*x, *y};
    }

    template <ByteOrder Order, typename T = f32> auto ReadVec3() -> std::optional<glm::vec<3, T>> {
        const auto x = Read<Order, T>();
        const auto y = Read<Order, T>();
        const auto z = Read<Order, T>();

        if (!x.has_value() || !y.has_value() || !z.has_value()) {
            return std::nullopt;
        }

        return glm::vec<3, T>{*x, *y, *z};
    }

    template <ByteOrder Order, typename T = f32> auto ReadVec4() -> std::optional<glm::vec<4, T>> {
        const auto x = Read<Order, T>();
        const auto y = Read<Order, T>();
        const auto z = Read<Order, T>();
        const auto w = Read<Order, T>();

        if (!x.has_value() || !y.has_value() || !z.has_value() || !w.has_value()) {
            return std::nullopt;
        }

        return glm::vec<4, T>{*x, *y, *z, *w};
    }

    template <ByteOrder Order, typename T = f32> auto ReadQuat() -> std::optional<glm::qua<T>> {
        const auto x = Read<Order, T>();
        const auto y = Read<Order, T>();
        const auto z = Read<Order, T>();
        const auto w = Read<Order, T>();

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
