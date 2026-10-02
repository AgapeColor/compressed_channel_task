#include "block_codec.hpp"
#include "transport_block_header.hpp"

#include <lz4.h>
#include <stdexcept>

std::size_t BlockCodec::compress(const std::byte* source, std::size_t size,
    std::vector<std::byte>& destination) {
    if (source == nullptr || size == 0 || size > maxBlockSize) {
        throw std::invalid_argument(
            "BlockCodec::compress(): invalid input");
    }

    const auto inputSize = static_cast<int>(size);
    const int capacity = LZ4_compressBound(inputSize);

    if (capacity <= 0) {
        throw std::runtime_error(
            "BlockCodec::compress(): cannot determine output capacity");
    }

    destination.resize(static_cast<std::size_t>(capacity));

    const int compressedSize = LZ4_compress_default(
        reinterpret_cast<const char*>(source),
        reinterpret_cast<char*>(destination.data()),
        inputSize,
        capacity);

    if (compressedSize <= 0) {
        throw std::runtime_error(
            "BlockCodec::compress(): compression failed");
    }

    destination.resize(static_cast<std::size_t>(compressedSize));
    return destination.size();
}

void BlockCodec::decompress(
    const std::vector<std::byte>& source,
    std::size_t originalSize,
    std::vector<std::byte>& destination) {
    if (&source == &destination) {
        throw std::invalid_argument(
            "BlockCodec::decompress(): buffers must be distinct");
    }

    if (source.empty() || originalSize == 0 ||
        originalSize > maxBlockSize || source.size() >= originalSize) {
        throw std::invalid_argument(
            "BlockCodec::decompress(): invalid block sizes");
    }

    destination.resize(originalSize);

    const int decodedSize = LZ4_decompress_safe(
        reinterpret_cast<const char*>(source.data()),
        reinterpret_cast<char*>(destination.data()),
        static_cast<int>(source.size()),
        static_cast<int>(originalSize));

    if (decodedSize < 0) {
        throw std::runtime_error(
            "BlockCodec::decompress(): decompression failed");
    }

    if (static_cast<std::size_t>(decodedSize) != originalSize) {
        throw std::runtime_error(
            "BlockCodec::decompress(): unexpected decoded size");
    }
}