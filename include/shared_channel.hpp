#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

enum class ChannelState : std::uint32_t { Empty, Ready, Finished };

struct SharedChannel {
    std::atomic<ChannelState> state{ChannelState::Empty};
    std::uint32_t size{0};
    std::byte data[248];
};

static_assert(sizeof(SharedChannel) == 256, "Error: SharedChannel must be exactly 256 bytes");

static_assert(std::atomic<ChannelState>::is_always_lock_free,
              "Error: ChannelState must use lock-free atomic operations");
