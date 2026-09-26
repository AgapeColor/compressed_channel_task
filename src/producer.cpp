#include "producer.hpp"

#include <iostream>
#include <cstring>
#include <thread>
#include <exception>
#include <stdexcept>

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

    const char* messages[] = {"Hello", "from", "producer"};

    for (const char* message : messages) {
        const auto length = std::strlen(message);

        std::memcpy(channel.data, message, length);
        channel.size = static_cast<std::uint32_t>(length);
        channel.state.store(ChannelState::Ready, std::memory_order_release);

        while (channel.state.load(std::memory_order_acquire) != ChannelState::Empty) {
            std::this_thread::yield();
        }
    }

    channel.state.store(ChannelState::Finished, std::memory_order_release);

    while (channel.state.load(std::memory_order_acquire) != ChannelState::Empty) {
        std::this_thread::yield();
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
