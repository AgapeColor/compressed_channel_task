#include "consumer.hpp"
#include "block_codec.hpp"
#include "thread_pool.hpp"

#include <deque>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

Consumer::Consumer(const std::string& outputPath)
    : memory_("/compressed_channel", SharedMemoryChannel::Mode::Open),
      output_(outputPath, std::ios::binary | std::ios::trunc) {
    if (!output_.is_open()) {
        memory_.abort();
        throw std::runtime_error("Consumer::Consumer(): Cannot open output file: " + outputPath);
    }
}

void Consumer::run() {
    try {
        constexpr std::size_t workerCount = 2;
        constexpr std::size_t maxInFlight = 4;

        std::unique_ptr<ThreadPool> pool;
        std::deque<std::future<std::vector<std::byte>>> pending;

        std::vector<std::byte> buffer;
        buffer.reserve(maxBlockSize);

        const auto writeBlock = [&](const std::vector<std::byte>& data) {
            output_.write(
                reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size()));

            if (!output_) {
                throw std::runtime_error(
                    "Consumer::run(): error writing output file");
            }
        };

        const auto writeNext = [&] {
            auto block = pending.front().get();
            pending.pop_front();

            writeBlock(block);
        };

        while (true) {
            const auto header = memory_.receiveBlock(buffer);

            if (!header.has_value()) {
                break;
            }

            const bool compressed = header->encodedSize < header->originalSize;

            if (!compressed && pending.empty()) {
                writeBlock(buffer);
                continue;
            }

            if (compressed) {
                if (!pool) {
                    pool = std::make_unique<ThreadPool>(workerCount, maxInFlight);
                }

                auto result = pool->submit(
                    [data = std::move(buffer), originalSize = header->originalSize] {
                        std::vector<std::byte> restored;
                        BlockCodec::decompress(data, originalSize, restored);
                        return restored;
                    });

                pending.push_back(std::move(result));
            } else {
                // Keep raw blocks behind earlier blocks awaiting decompression.
                std::promise<std::vector<std::byte>> ready;
                auto result = ready.get_future();
                ready.set_value(std::move(buffer));
                pending.push_back(std::move(result));
            }

            if (pending.size() >= maxInFlight) {
                writeNext();
            }
        }

        while (!pending.empty()) {
            writeNext();
        }

        output_.flush();

        if (!output_) {
            throw std::runtime_error("Consumer::run(): error flushing output file");
        }

        memory_.acknowledgeFinished();
    } catch (...) {
        memory_.abort();
        throw;
    }
    
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <output-file>\n";
        return 1;
    }

    try {
        Consumer consumer(argv[1]);
        consumer.run();
    } catch (const std::exception& error) {
        std::cerr << "Consumer: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
