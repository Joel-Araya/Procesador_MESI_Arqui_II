#include "memory.h"
#include <cstring>
#include <stdexcept>

Memory::Memory() {
    std::fill(mem_.begin(), mem_.end(), 0);
}

void Memory::write_block(uint64_t address, const uint8_t* data) {
    if (address >= MEM_SIZE * WORD_SIZE)
        throw std::out_of_range("Dirección fuera de rango de memoria");
    std::memcpy(&mem_[address], data, WORD_SIZE);
}

void Memory::read_block(uint64_t address, uint8_t* buffer) const {
    if (address >= MEM_SIZE * WORD_SIZE)
        throw std::out_of_range("Dirección fuera de rango de memoria");
    std::memcpy(buffer, &mem_[address], WORD_SIZE);
}
