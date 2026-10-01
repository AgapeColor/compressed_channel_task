#pragma once

#include "shared_channel.hpp"
#include "transport_block_header.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class SharedMemoryChannel {
  public:
    enum class Mode { Create, Open };

    SharedMemoryChannel(const std::string& name, Mode mode);
    ~SharedMemoryChannel();

    SharedMemoryChannel(const SharedMemoryChannel&) = delete;
    SharedMemoryChannel& operator=(const SharedMemoryChannel&) = delete;

    void finish();
    void acknowledgeFinished();
    void abort() noexcept;

    void sendBlock(
      const std::byte* data,
      std::size_t encodedSize,
      std::size_t originalSize);

  std::optional<TransportBlockHeader> receiveBlock(
    std::vector<std::byte>& destination);

  private:
    void cleanup() noexcept;
    void changeState(ChannelState expected, ChannelState desired);
    void waitForState(ChannelState desired);
    void send(const std::byte* data, std::size_t size);
    std::optional<std::size_t> receive(std::byte* destination, std::size_t capacity);
    std::size_t payloadCapacity() const noexcept;

    std::string name_;
    int fd_ = -1;
    SharedChannel* channel_ = nullptr;
    bool isOwner_ = false;
};
