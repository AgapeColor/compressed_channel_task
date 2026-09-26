#include "consumer.hpp"

#include <exception>
#include <iostream>
#include <thread>

Consumer::Consumer() 
    : memory_("/compressed_channel", SharedMemory::Mode::Open)
{}

void Consumer::run() {
    auto& channel = memory_.channel();

    while (true) {
        const auto state = channel.state.load(std::memory_order_acquire);

        if (state == ChannelState::Finished) {
            channel.state.store(ChannelState::Empty, std::memory_order_release);
            break;
        }

        if (state != ChannelState::Ready) {
            std::this_thread::yield();
            continue;
        }

        std::cout.write(
            reinterpret_cast<const char*>(channel.data),
            static_cast<std::streamsize>(channel.size));
        std::cout.put('\n');
        
        channel.state.store(ChannelState::Empty, std::memory_order_release);
    }
}

int main() {
    try {
        Consumer consumer;
        consumer.run();
    } catch (const std::exception& error) {
        std::cerr << "Consumer: " << error.what() << '\n';
        return 1;
    }
    
    return 0;
}


