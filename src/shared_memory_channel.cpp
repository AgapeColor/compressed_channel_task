#include "shared_memory_channel.hpp"

#include <algorithm>
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

void SharedMemoryChannel::finish() {
    waitForState(ChannelState::Empty);

    changeState(ChannelState::Empty, ChannelState::Finished);

    waitForState(ChannelState::Empty);
}

void SharedMemoryChannel::acknowledgeFinished() {
    changeState(ChannelState::Finished, ChannelState::Empty);
}

void SharedMemoryChannel::abort() noexcept {
    channel_->state.store(ChannelState::Aborted, std::memory_order_release);
}

void SharedMemoryChannel::sendBlock(const std::byte* data, 
                                    std::size_t encodedSize,
                                    std::size_t originalSize) {
    if (data == nullptr) {
        throw std::invalid_argument(
            "SharedMemoryChannel::sendBlock(): data must not be null");
    }

    if (originalSize == 0 || originalSize > maxBlockSize ||
        encodedSize == 0 || encodedSize > originalSize) {
        throw std::length_error(
            "SharedMemoryChannel::sendBlock(): invalid block sizes");
    }

    const TransportBlockHeader header{
        static_cast<std::uint32_t>(originalSize),
        static_cast<std::uint32_t>(encodedSize)
    };

    send(reinterpret_cast<const std::byte*>(&header),sizeof(header));

    std::size_t offset = 0;

    while (offset < encodedSize) {
        const auto packetSize =
            std::min(payloadCapacity(), encodedSize - offset);

        send(data + offset, packetSize);
        offset += packetSize;
    }
}

std::optional<TransportBlockHeader> SharedMemoryChannel::receiveBlock(std::vector<std::byte>& destination) {
    TransportBlockHeader header{};

    const auto headerSize = receive(
        reinterpret_cast<std::byte*>(&header),
        sizeof(header));

    if (!headerSize.has_value()) {
        return std::nullopt;
    }

    if (*headerSize != sizeof(header)) {
        throw std::runtime_error(
            "SharedMemoryChannel::receiveBlock(): invalid header size");
    }

    if (header.originalSize == 0 || header.originalSize > maxBlockSize ||
        header.encodedSize == 0 || header.encodedSize > header.originalSize) {
        throw std::runtime_error(
            "SharedMemoryChannel::receiveBlock(): invalid block sizes");
    }

    destination.resize(header.encodedSize);

    std::size_t offset = 0;

    while (offset < destination.size()) {
        const auto remaining = destination.size() - offset;
        const auto expectedSize = std::min(payloadCapacity(), remaining);

        const auto received = receive(
            destination.data() + offset,
            remaining);

        if (!received.has_value()) {
            throw std::runtime_error(
                "SharedMemoryChannel::receiveBlock(): transfer ended inside block");
        }

        if (*received != expectedSize) {
            throw std::runtime_error(
                "SharedMemoryChannel::receiveBlock(): invalid fragment size");
        }

        offset += *received;
    }

    return header;
}

void SharedMemoryChannel::cleanup() noexcept {
    if (channel_ != nullptr) {
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

void SharedMemoryChannel::changeState(ChannelState expected, ChannelState desired) {
    const bool changed = channel_->state.compare_exchange_strong(
        expected,
        desired,
        std::memory_order_release,
        std::memory_order_relaxed);

    if (!changed) {
        if (expected == ChannelState::Aborted) {
            throw std::runtime_error(
                "SharedMemoryChannel::changeState(): transfer aborted");
        }

        throw std::runtime_error(
            "SharedMemoryChannel::changeState(): unexpected channel state");
    }
}

void SharedMemoryChannel::waitForState(ChannelState desired) {
    while (true) {
        const auto state = channel_->state.load(std::memory_order_acquire);

        if (state == ChannelState::Aborted) {
            throw std::runtime_error(
                "SharedMemoryChannel::waitForState(): transfer aborted");
        }

        if (state == desired) {
            return;
        }

        std::this_thread::yield();
    }
}

void SharedMemoryChannel::send(const std::byte* data, std::size_t size) {
    if (data == nullptr) {
        throw std::invalid_argument("SharedMemoryChannel::send(): data must not be null");
    }

    if (size == 0 || size > payloadCapacity()) {
        throw std::length_error("SharedMemoryChannel::send(): payload size is invalid");
    }

    waitForState(ChannelState::Empty);

    std::memcpy(channel_->data, data, size);

    channel_->size = static_cast<std::uint32_t>(size);

    changeState(ChannelState::Empty, ChannelState::Ready);
}

std::optional<std::size_t> SharedMemoryChannel::receive(std::byte* destination,
                                                        std::size_t capacity) {
    if (destination == nullptr) {
        throw std::invalid_argument("SharedMemoryChannel::receive(): destination must not be null");
    }

    if (capacity == 0) {
        throw std::invalid_argument("SharedMemoryChannel::receive(): capacity must not be zero");
    }

    while (true) {
        const auto state = channel_->state.load(std::memory_order_acquire);

        if (state == ChannelState::Aborted) {
            throw std::runtime_error("SharedMemoryChannel::receive(): transfer aborted");
        }

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

    if (size > capacity) {
        throw std::length_error("SharedMemoryChannel::receive(): destination buffer is too small");
    }

    std::memcpy(destination, channel_->data, size);

    changeState(ChannelState::Ready, ChannelState::Empty);

    return size;
}

std::size_t SharedMemoryChannel::payloadCapacity() const noexcept { return sizeof(channel_->data); }
