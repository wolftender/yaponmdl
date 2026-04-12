#include <deque>
#include <cstdint>
#include <optional>
#include <vector>

namespace util {

template <typename T> class GenerationalPool {
private:
    struct Slot {
        uint32_t generation = 0;
        std::optional<T> value = std::nullopt;
    };

    static auto MakeId(uint32_t index, uint32_t generation) {
        return static_cast<uint64_t>(index) | (static_cast<uint64_t>(generation) << 32);
    }

    static auto UnmakeId(uint64_t id) -> std::pair<uint32_t, uint32_t> {
        const uint32_t mask = 0x00000000ffffffff;
        const auto index = static_cast<uint32_t>(id & mask);
        const auto generation = static_cast<uint32_t>((id >> 32) & mask);

        return {index, generation};
    }

public:
    GenerationalPool() = default;
    ~GenerationalPool() = default;

    auto Insert(T &&value) -> uint64_t {
        if (free_list_.empty()) {
            slots_.emplace_back(
                Slot{
                    .generation = 0,
                    .value = std::move(value),
                });

            return MakeId(slots_.size() - 1, 0);
        }

        const auto free_index = free_list_.front();
        free_list_.pop_front();

        slots_[free_index].value = std::move(value);
        return MakeId(free_index, slots_[free_index].generation);
    }

    auto Get(uint64_t id) -> T * {
        const auto [index, generation] = UnmakeId(id);
        if (index >= slots_.size()) {
            return nullptr;
        }

        if (generation != slots_[index].generation || !slots_[index].value.has_value()) {
            return nullptr;
        }

        return &slots_[index].value.value();
    }

    auto Get(uint64_t id) const -> const T * {
        const auto [index, generation] = UnmakeId(id);
        if (index >= slots_.size()) {
            return nullptr;
        }

        if (generation != slots_[index].generation || !slots_[index].value.has_value()) {
            return nullptr;
        }

        return &slots_[index].value.value();
    }

    auto Erase(uint64_t id) -> void {
        const auto [index, generation] = UnmakeId(id);
        if (index >= slots_.size()) {
            return;
        }

        if (generation != slots_[index].generation || !slots_[index].value.has_value()) {
            return;
        }

        slots_[index].value = std::nullopt;
        slots_[index].generation++;
    }

private:
    std::vector<Slot> slots_;
    std::deque<uint32_t> free_list_;
};

} // namespace util