#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace common {

// nick: for client/server state snapshots
template <typename T, std::size_t Capacity> class SequenceBuffer {
    static_assert(Capacity > 0, "Capacity must be > 0");

  public:
    struct Entry {
        std::uint32_t sequence = 0;
        bool valid = false;
        T value{};
    };

    static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

    void clear() noexcept {
        for (auto& e : entries_) {
            e.valid = false;
            e.sequence = 0;
        }
    }

    T& insert(std::uint32_t sequence, const T& value) {
        Entry& e = entries_[index_of(sequence)];
        e.sequence = sequence;
        e.valid = true;
        e.value = value;
        return e.value;
    }

    T& insert(std::uint32_t sequence, T&& value) {
        Entry& e = entries_[index_of(sequence)];
        e.sequence = sequence;
        e.valid = true;
        e.value = std::move(value);
        return e.value;
    }

    template <typename... Args> T& emplace(std::uint32_t sequence, Args&&... args) {
        Entry& e = entries_[index_of(sequence)];
        e.sequence = sequence;
        e.valid = true;
        e.value = T(std::forward<Args>(args)...);
        return e.value;
    }

    T* find(std::uint32_t sequence) noexcept {
        Entry& e = entries_[index_of(sequence)];
        if (!e.valid || e.sequence != sequence) {
            return nullptr;
        }
        return &e.value;
    }

    const T* find(std::uint32_t sequence) const noexcept {
        const Entry& e = entries_[index_of(sequence)];
        if (!e.valid || e.sequence != sequence) {
            return nullptr;
        }
        return &e.value;
    }

    bool contains(std::uint32_t sequence) const noexcept {
        return find(sequence) != nullptr;
    }

    Entry* find_entry(std::uint32_t sequence) noexcept {
        Entry& e = entries_[index_of(sequence)];
        if (!e.valid || e.sequence != sequence) {
            return nullptr;
        }
        return &e;
    }

    const Entry* find_entry(std::uint32_t sequence) const noexcept {
        const Entry& e = entries_[index_of(sequence)];
        if (!e.valid || e.sequence != sequence) {
            return nullptr;
        }
        return &e;
    }

  private:
    std::array<Entry, Capacity> entries_{};

    static constexpr bool is_power_of_two(std::size_t x) noexcept {
        return x && ((x & (x - 1)) == 0);
    }

    static constexpr std::size_t index_of(std::uint32_t sequence) noexcept {
        if constexpr (is_power_of_two(Capacity)) {
            return static_cast<std::size_t>(sequence & (Capacity - 1));
        } else {
            return static_cast<std::size_t>(sequence % Capacity);
        }
    }
};

} // namespace common
