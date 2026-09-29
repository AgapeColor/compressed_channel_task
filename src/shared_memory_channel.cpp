#include "shared_memory_channel.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <new>
#include <stdexcept>
#include <sys/mman.h>
#include <system_error>
#include <thread>
#include <unistd.h>

SharedMemoryChannel::SharedMemoryChannel(const std::string& name, Mode mode) : name_(name) {
    try {
        int flags = O_RDWR;
        if (mode == Mode::Create) {
            flags |= O_CREAT | O_EXCL;
        }

        fd_ = shm_open(name_.c_str(), flags, 0600);
        if (fd_ == -1) {
            throw std::system_error(errno, std::generic_category(), "shm_open");
        }

        isOwner_ = (mode == Mode::Create);

        if (isOwner_ && ftruncate(fd_, sizeof(SharedChannel)) == -1) {
            throw std::system_error(errno, std::generic_category(), "ftruncate");
        }

        void* memory =
            mmap(nullptr, sizeof(SharedChannel), PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);

        if (memory == MAP_FAILED) {
            throw std::system_error(errno, std::generic_category(), "mmap");
        }
        if (isOwner_) {
            channel_ = new (memory) SharedChannel{};
        } else {
            channel_ = static_cast<SharedChannel*>(memory);
        }
    } catch (...) {
        cleanup();
        throw;
    }
}

SharedMemoryChannel::~SharedMemoryChannel() { cleanup(); }

std::size_t SharedMemoryChannel::payloadCapacity() const noexcept { return sizeof(channel_->data); }

void SharedMemoryChannel::send(const std::byte* data, std::size_t size) {
    if (data == nullptr) {
        throw std::invalid_argument("SharedMemoryChannel::send(): data must not be null");
    }

    if (size == 0 || size > payloadCapacity()) {
        throw std::length_error("SharedMemoryChannel::send(): payload size is invalid");
    }

    while (channel_->state.load(std::memory_order_acquire) != ChannelState::Empty) {
        std::this_thread::yield();
    }

    std::memcpy(channel_->data, data, size);

    channel_->size = static_cast<std::uint32_t>(size);
    channel_->state.store(ChannelState::Ready, std::memory_order_release);
}

std::optional<std::size_t> SharedMemoryChannel::receive(std::byte* destination,
                                                        std::size_t capacity) {
    if (destination == nullptr) {
        throw std::invalid_argument("SharedMemoryChannel::receive(): destination must not be null");
    }

    if (capacity < payloadCapacity()) {
        throw std::length_error("SharedMemoryChannel::receive(): destination buffer is too small");
    }

    while (true) {
        const auto state = channel_->state.load(std::memory_order_acquire);

        if (state == ChannelState::Finished) {
            return std::nullopt;
        }

        if (state != ChannelState::Ready) {
            std::this_thread::yield();
            continue;
        }

        break;
    }

    const auto size = static_cast<std::size_t>(channel_->size);

    if (size == 0 || size > payloadCapacity()) {
        throw std::runtime_error("SharedMemoryChannel::receive(): invalid payload size");
    }

    std::memcpy(destination, channel_->data, size);

    channel_->state.store(ChannelState::Empty, std::memory_order_release);

    return size;
}

void SharedMemoryChannel::finish() {
    while (channel_->state.load(std::memory_order_acquire) != ChannelState::Empty) {
        std::this_thread::yield();
    }

    channel_->state.store(ChannelState::Finished, std::memory_order_release);

    while (channel_->state.load(std::memory_order_acquire) != ChannelState::Empty) {
        std::this_thread::yield();
    }
}

void SharedMemoryChannel::acknowledgeFinished() {
    channel_->state.store(ChannelState::Empty, std::memory_order_release);
}

void SharedMemoryChannel::cleanup() noexcept {
    if (channel_ != nullptr) {
        if (isOwner_) {
            channel_->~SharedChannel();
        }

        munmap(channel_, sizeof(SharedChannel));
        channel_ = nullptr;
    }

    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }

    if (isOwner_) {
        shm_unlink(name_.c_str());
        isOwner_ = false;
    }
}
