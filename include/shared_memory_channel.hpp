#pragma once

#include "shared_channel.hpp"

#include <cstddef>
#include <optional>
#include <string>

class SharedMemoryChannel {
public:
    enum class Mode {
        Create,
        Open
    };

    SharedMemoryChannel(const std::string& name, Mode mode);
    ~SharedMemoryChannel();

    SharedMemoryChannel(const SharedMemoryChannel&) = delete;
    SharedMemoryChannel& operator=(const SharedMemoryChannel&) = delete;

    std::size_t payloadCapacity() const noexcept;

    void send(const std::byte* data, std::size_t size);

    std::optional<std::size_t> receive(std::byte* destination, std::size_t capacity);

    void finish();

    void acknowledgeFinished();

private:
    void cleanup() noexcept;

    std::string name_;
    int fd_ = -1;
    SharedChannel* channel_ = nullptr;
    bool isOwner_ = false;
};
