#pragma once

#include <cstddef>
#include <vector>

class BlockCodec {
public:
    static std::size_t compress(
        const std::byte* source,
        std::size_t size,
        std::vector<std::byte>& destination);

    static void decompress(
        const std::vector<std::byte>& source,
        std::size_t originalSize,
        std::vector<std::byte>& destination);
};