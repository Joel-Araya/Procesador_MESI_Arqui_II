#pragma once
#include <vector>
#include <cstdint>
#include <mutex>

class SharedMemory {
public:
    virtual ~SharedMemory() = default; // Virtual destructor
    virtual uint64_t load(uint64_t address) = 0;
    virtual void store(uint64_t address, uint64_t value) = 0;
};
