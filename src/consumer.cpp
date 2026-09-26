#include "consumer.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>

Consumer::Consumer(const std::string& outputPath)
    : memory_("/compressed_channel", SharedMemory::Mode::Open),
      output_(outputPath, std::ios::binary | std::ios::trunc)
{
    if (!output_.is_open()) {
        throw std::runtime_error("Cannot open output file: " + outputPath);
    }
}

void Consumer::run() {
    auto& channel = memory_.channel();

    while (true) {
        const auto state = channel.state.load(std::memory_order_acquire);

        if (state == ChannelState::Finished) {
            break;
        }

        if (state != ChannelState::Ready) {
            std::this_thread::yield();
            continue;
        }

        output_.write(
            reinterpret_cast<const char*>(channel.data),
            static_cast<std::streamsize>(channel.size)
        );

        channel.state.store(ChannelState::Empty, std::memory_order_release);
    }

    output_.flush();
    const bool writeFailed = !output_;

    channel.state.store(ChannelState::Empty, std::memory_order_release);

    if (writeFailed) {
        throw std::runtime_error("Error writing output file");
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


