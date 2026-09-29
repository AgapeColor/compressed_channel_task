#pragma once

#include "shared_memory_channel.hpp"

#include <fstream>
#include <string>

class Consumer {
public:
    explicit Consumer(const std::string& outputPath);

    void run();

private:
    SharedMemoryChannel memory_;
    std::ofstream output_;
};
