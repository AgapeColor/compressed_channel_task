#pragma once

#include "shared_memory.hpp"

#include <fstream>
#include <string>

class Consumer {
public:
    explicit Consumer(const std::string& outputPath);

    void run();

private:
    SharedMemory memory_;
    std::ofstream output_;
};
