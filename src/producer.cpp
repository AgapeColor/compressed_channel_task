#include "shared_memory.hpp"

#include <iostream>
#include <cstring>
#include <thread>
#include <fstream>
#include <exception>

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <input-file>\n";
            return 1;
        }

        std::ifstream input(argv[1], std::ios::binary);
        if (!input.is_open()) {
            std::cerr << "Cannot open file: " << argv[1] << '\n';
            return 1;
        }

        SharedMemory memory("/compressed_channel", SharedMemory::Mode::Create);
        auto* channel = &memory.channel();

        const char* messages[] = {"Hello", "from", "producer"};

        for (const char* message : messages) {
            const auto length = std::strlen(message);

            std::memcpy(channel->data, message, length);
            channel->size = static_cast<std::uint32_t>(length);
            channel->state.store(ChannelState::Ready,
                                std::memory_order_release);

            while (channel->state.load(std::memory_order_acquire) != ChannelState::Empty) {
                std::this_thread::yield();
            }
        }

        channel->state.store(ChannelState::Finished,
                            std::memory_order_release);

        while (channel->state.load(std::memory_order_acquire) != ChannelState::Empty) {
            std::this_thread::yield();
        }

    } catch (const std::exception& error) {
        std::cerr << "Producer: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
