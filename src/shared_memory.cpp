#include "shared_memory.hpp"

#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <system_error>
#include <cerrno>
#include <new>

SharedMemory::SharedMemory(const std::string& name, Mode mode)
    : name_(name) {
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

        void* memory = mmap(
            nullptr,
            sizeof(SharedChannel),
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd_,
            0
        );

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

SharedMemory::~SharedMemory() {
    cleanup();
}

SharedChannel& SharedMemory::channel() noexcept {
    return *channel_;
}

void SharedMemory::cleanup() noexcept {
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
