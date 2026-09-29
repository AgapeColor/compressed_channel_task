#include "producer.hpp"

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
      memory_("/compressed_channel", SharedMemoryChannel::Mode::Create)
{}

void Producer::run() {
    std::vector<std::byte> buffer(memory_.payloadCapacity());

    while (true) {
        input_.read(
            reinterpret_cast<char*>(buffer.data()),
            static_cast<std::streamsize>(buffer.size())
        );

        const auto bytesRead = input_.gcount();
        if (bytesRead == 0) {
            break;
        }

        memory_.send(
            buffer.data(),
            static_cast<std::size_t>(bytesRead)
        );
    }

    const bool readFailed = input_.bad() || (input_.fail() && !input_.eof());

    memory_.finish();
    if (readFailed) {
        throw std::runtime_error("Producer::run(): error reading input file");
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input-file>\n";
        return 1;
    }

    try {
        Producer producer(argv[1]);
        producer.run();
    } catch (const std::exception& error) {
        std::cerr << "Producer: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
