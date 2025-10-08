#ifndef MEMORY_H
#define MEMORY_H

#include <array>
#include <cstdint>
#include <iostream>

class Memory {
public:
    static const int MEM_SIZE = 512; // 512 posiciones de 64 bits
    static const int WORD_SIZE = 8;  // 8 bytes = 64 bits

    Memory();

    void write_block(uint64_t address, const uint8_t* data);
    void read_block(uint64_t address, uint8_t* buffer) const;

private:
    std::array<uint8_t, MEM_SIZE * WORD_SIZE> mem_;
};

#endif
