#include "producer.hpp"
#include "block_codec.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
std::ifstream openInputFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);

    if (!input.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    return input;
}
}

Producer::Producer(const std::string& inputPath)
    : input_(openInputFile(inputPath)),
      memory_("/compressed_channel", SharedMemoryChannel::Mode::Create) {}

void Producer::run(bool useCompression) {
    try {
        const auto started = std::chrono::steady_clock::now();

        std::uint64_t originalBytes = 0;
        std::uint64_t encodedBytes = 0;
        std::uint64_t blockCount = 0;
        std::uint64_t compressedBlockCount = 0;

        std::vector<std::byte> buffer(maxBlockSize);
        std::vector<std::byte> compressed;

        while (true) {
            input_.read(reinterpret_cast<char*>(buffer.data()),
                        static_cast<std::streamsize>(buffer.size()));

            const auto bytesRead = input_.gcount();
            if (bytesRead == 0) {
                break;
            }

            const auto blockSize = static_cast<std::size_t>(bytesRead);

            const std::byte* payload = buffer.data();
            std::size_t encodedSize = blockSize;

            if (useCompression) {
                const auto compressedSize = BlockCodec::compress(
                    buffer.data(), blockSize, compressed);

                if (compressedSize < blockSize) {
                    payload = compressed.data();
                    encodedSize = compressedSize;
                }
            }

            memory_.sendBlock(payload, encodedSize, blockSize);

            originalBytes += blockSize;
            encodedBytes += encodedSize;
            ++blockCount;

            if (encodedSize < blockSize) {
                ++compressedBlockCount;
            }
        }

        const bool readFailed = input_.bad() || (input_.fail() && !input_.eof());


        if (readFailed) {
            throw std::runtime_error("Producer::run(): error reading input file");
        }
        memory_.finish();

        const auto finished = std::chrono::steady_clock::now();

        const double elapsedSeconds =
            std::chrono::duration<double>(finished - started).count();

        const auto headerBytes =
            blockCount * sizeof(TransportBlockHeader);

        std::cout
            << "Mode: " << (useCompression ? "lz4" : "raw") << '\n'
            << "Original bytes: " << originalBytes << '\n'
            << "Encoded bytes: " << encodedBytes << '\n'
            << "Transferred bytes including block headers: "
            << encodedBytes + headerBytes << '\n'
            << "Blocks: " << blockCount << '\n'
            << "Compressed blocks: " << compressedBlockCount << '\n'
            << "Elapsed seconds: " << elapsedSeconds << '\n';

        if (originalBytes != 0) {
            const double compressionRatio =
                static_cast<double>(originalBytes) /
                static_cast<double>(encodedBytes);

            const double savedPercent =
                100.0 * (1.0 -
                        static_cast<double>(encodedBytes) /
                        static_cast<double>(originalBytes));

            std::cout
                << "Compression ratio: " << compressionRatio << '\n'
                << "Payload reduction percent: " << savedPercent << '\n';
        }

    } catch (...) {
        memory_.abort();
        throw;
    }
}

int main(int argc, char* argv[]) {
    if ((argc != 2 && argc != 3) ||
        (argc == 3 && std::string(argv[2]) != "--raw")) {
        std::cerr << "Usage: " << argv[0] << " <input-file> [--raw]\n";
        return 1;
    }

    try {
        Producer producer(argv[1]);
        producer.run(argc == 2);
    } catch (const std::exception& error) {
        std::cerr << "Producer: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
