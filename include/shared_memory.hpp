#pragma once

#include "shared_channel.hpp"

#include <string>

class SharedMemory {
public:
    enum class Mode {
        Create,
        Open
    };

    SharedMemory(const std::string& name, Mode mode);
    ~SharedMemory();

    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    SharedChannel& channel() noexcept;

private:
    void cleanup() noexcept;

    std::string name_;
    int fd_ = -1;
    SharedChannel* channel_ = nullptr;
    bool isOwner_ = false;
};
