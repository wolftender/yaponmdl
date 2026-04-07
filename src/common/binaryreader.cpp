#include "binaryreader.hpp"

namespace util::bytes {

auto BinaryReader::Remaining() const -> u64 { return data_span_.size() - ptr_; }

auto BinaryReader::Seek(u64 location) -> Result {
    if (location > data_span_.size()) {
        return Result::eFailure;
    }

    ptr_ = location;
    return Result::eSuccess;
}

auto BinaryReader::ReadBuffer(u64 num_bytes) -> std::optional<std::span<const u8>> {
    const auto end_ptr = data_span_.size();

    if (ptr_ + num_bytes > end_ptr) {
        return std::nullopt;
    }

    const auto data_ptr = data_span_.data() + ptr_;
    ptr_ += num_bytes;

    return std::span<const u8>{data_ptr, num_bytes};
}

} // namespace util::bytes