#include "producer.hpp"
#include "block_codec.hpp"
#include "thread_pool.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
struct EncodedBlock {
    std::size_t originalSize;
    std::vector<std::byte> data;
};

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

        constexpr std::size_t workerCount = 2;
        constexpr std::size_t maxInFlight = 4;

        std::unique_ptr<ThreadPool> pool;
        if (useCompression) {
            pool = std::make_unique<ThreadPool>(workerCount, maxInFlight);
        }

        std::deque<std::future<EncodedBlock>> pending;

        const auto sendBlock = [&](const std::vector<std::byte>& data,
                                   std::size_t originalSize) {
            memory_.sendBlock(data.data(), data.size(), originalSize);

            originalBytes += originalSize;
            encodedBytes += data.size();
            ++blockCount;

            if (data.size() < originalSize) {
                ++compressedBlockCount;
            }
        };

        const auto sendNext = [&] {
            auto block = pending.front().get();
            pending.pop_front();

            sendBlock(block.data, block.originalSize);
        };

        while (true) {
            std::vector<std::byte> buffer(maxBlockSize);

            input_.read(reinterpret_cast<char*>(buffer.data()),
                        static_cast<std::streamsize>(buffer.size()));

            const auto bytesRead = input_.gcount();
            if (bytesRead == 0) {
                break;
            }

            buffer.resize(static_cast<std::size_t>(bytesRead));

            if (!useCompression) {
                sendBlock(buffer, buffer.size());
                continue;
            }

            auto result = pool->submit(
                [data = std::move(buffer)]() mutable -> EncodedBlock {
                    const auto originalSize = data.size();

                    std::vector<std::byte> compressed;
                    const auto compressedSize = BlockCodec::compress(
                        data.data(), originalSize, compressed);

                    if (compressedSize < originalSize) {
                        return EncodedBlock{originalSize, std::move(compressed)};
                    }

                    return EncodedBlock{originalSize, std::move(data)};
                });

            pending.push_back(std::move(result));

            if (pending.size() >= maxInFlight) {
                sendNext();
            }
        }

        const bool readFailed = input_.bad() || (input_.fail() && !input_.eof());


        if (readFailed) {
            throw std::runtime_error("Producer::run(): error reading input file");
        }

        while (!pending.empty()) {
            sendNext();
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
