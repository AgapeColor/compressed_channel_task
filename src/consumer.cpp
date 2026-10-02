#include "consumer.hpp"
#include "block_codec.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
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
        std::vector<std::byte> buffer;
        buffer.reserve(maxBlockSize);

        std::vector<std::byte> restored;
        restored.reserve(maxBlockSize);

        while (true) {
            const auto header = memory_.receiveBlock(buffer);

            if (!header.has_value()) {
                break;
            }

            const bool compressed = header->encodedSize < header->originalSize;

            if (compressed) {
                BlockCodec::decompress(buffer, header->originalSize, restored);
            }

            const auto& outputBuffer = compressed ? restored : buffer;

            output_.write(
                reinterpret_cast<const char*>(outputBuffer.data()),
                static_cast<std::streamsize>(outputBuffer.size()));
        
            if (!output_) {
                throw std::runtime_error(
                    "Consumer::run(): error writing output file");
            }
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
