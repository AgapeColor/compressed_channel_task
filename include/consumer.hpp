#pragma once

#include "shared_memory.hpp"

class Consumer {
public:
    Consumer();

    void run();

private:
    SharedMemory memory_;
};