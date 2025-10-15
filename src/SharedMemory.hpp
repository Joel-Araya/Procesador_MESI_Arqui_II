#pragma once
#include <vector>
#include <cstdint>
#include <mutex>

class SharedMemory {
public:
    explicit SharedMemory(size_t sizeBytes) : m_data(sizeBytes/8, 0) {}

    uint64_t load(uint64_t addr) {
        std::scoped_lock lock(m_mutex);
        size_t idx = addr/8;
        if (idx >= m_data.size()) return 0;
        return m_data[idx];
    }
    void store(uint64_t addr, uint64_t val) {
        std::scoped_lock lock(m_mutex);
        size_t idx = addr/8;
        if (idx >= m_data.size()) return;
        m_data[idx] = val;
    }
private:
    std::vector<uint64_t> m_data; // simple word-addressable 64-bit
    std::mutex m_mutex;
};
