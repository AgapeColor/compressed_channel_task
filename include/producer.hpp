#pragma once

#include "shared_memory.hpp"

#include <fstream>
#include <string>

class Producer {
public:
    explicit Producer(const std::string& inputPath);

    void run();

private:
    std::ifstream input_;
    SharedMemory memory_;
};
