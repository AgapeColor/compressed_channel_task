#include "producer.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>

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
      memory_("/compressed_channel", SharedMemory::Mode::Create)
{}

void Producer::run() {
    auto& channel = memory_.channel();

    while (true) {
        input_.read(reinterpret_cast<char*>(channel.data), sizeof(channel.data));

        const auto bytesRead = input_.gcount();

        if (bytesRead == 0) {
            break;
        }

        channel.size = static_cast<std::uint32_t>(bytesRead);
        channel.state.store(ChannelState::Ready, std::memory_order_release);

        while (channel.state.load(std::memory_order_acquire) != ChannelState::Empty) {
            std::this_thread::yield();
        }
    }

    const bool readFailed = input_.bad() || (input_.fail() && !input_.eof());

    channel.state.store(ChannelState::Finished, std::memory_order_release);

    while (channel.state.load(std::memory_order_acquire) != ChannelState::Empty) {
        std::this_thread::yield();
    }

    if (readFailed) {
        throw std::runtime_error("Error reading input file");
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
