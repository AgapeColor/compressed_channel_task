#include "consumer.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

Consumer::Consumer(const std::string& outputPath)
    : memory_("/compressed_channel", SharedMemoryChannel::Mode::Open),
      output_(outputPath, std::ios::binary | std::ios::trunc) {
    if (!output_.is_open()) {
        throw std::runtime_error("Cannot open output file: " + outputPath);
    }
}

void Consumer::run() {
    std::vector<std::byte> buffer(memory_.payloadCapacity());

    while (true) {
        const auto bytesReceived = memory_.receive(buffer.data(), buffer.size());

        if (!bytesReceived.has_value()) {
            break;
        }

        output_.write(reinterpret_cast<const char*>(buffer.data()),
                      static_cast<std::streamsize>(*bytesReceived));
    }

    output_.flush();
    const bool writeFailed = !output_;

    memory_.acknowledgeFinished();

    if (writeFailed) {
        throw std::runtime_error("Consumer::run(): error writing output file");
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
