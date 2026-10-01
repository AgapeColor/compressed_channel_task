#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

inline constexpr std::size_t maxBlockSize = std::size_t{64} * 1024;

struct TransportBlockHeader {
    std::uint32_t originalSize;
    std::uint32_t encodedSize;
};

static_assert(sizeof(TransportBlockHeader) == 8);
static_assert(std::is_trivially_copyable_v<TransportBlockHeader>);
