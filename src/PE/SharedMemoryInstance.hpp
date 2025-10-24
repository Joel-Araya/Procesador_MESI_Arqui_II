#pragma once
#include <vector>
#include <cstdint>
#include <mutex>
#include "SharedMemory.hpp"

class SharedMemoryInstance : public SharedMemory {
public:
    explicit SharedMemoryInstance(size_t sizeBytes) : m_data(sizeBytes/8, 0) {}

    uint64_t load(uint64_t addr) override {
        std::scoped_lock lock(m_mutex);
        size_t idx = addr/8;
        if (idx >= m_data.size()) return 0;
        return m_data[idx];
    }
    void store(uint64_t addr, uint64_t val) override {
        std::scoped_lock lock(m_mutex);
        size_t idx = addr/8;
        if (idx >= m_data.size()) return;
        m_data[idx] = val;
    }
private:
    std::vector<uint64_t> m_data; // simple word-addressable 64-bit
    std::mutex m_mutex;
};
