#include "memory.h"
#include <cstring>
#include <stdexcept>
#include <iomanip>

Memory::Memory() {
    mem_.fill(0);
}

uint64_t Memory::align_addr(uint64_t address) const {
    return (address / BLOCK_BYTES) * BLOCK_BYTES;
}

void Memory::read_block(uint64_t address, uint64_t* out_block) const {
    uint64_t base = align_addr(address);
    if (base + BLOCK_BYTES > MEM_BYTES) {
        throw std::out_of_range("Memory::read_block: address out of range");
    }
    std::memcpy(out_block, mem_.data() + base, BLOCK_BYTES);
    std::cout << "[MEM] read_block @ 0x" << std::hex << base << std::dec << " (32B)\n";
}

void Memory::write_block(uint64_t address, const uint64_t* in_block) {
    uint64_t base = align_addr(address);
    if (base + BLOCK_BYTES > MEM_BYTES) {
        throw std::out_of_range("Memory::write_block: address out of range");
    }
    std::memcpy(mem_.data() + base, in_block, BLOCK_BYTES);
    std::cout << "[MEM] write_block @ 0x" << std::hex << base << std::dec << " (32B)\n";
}

void Memory::read_word(uint64_t address, uint64_t* out_word) const {
    if (address + WORD_BYTES > MEM_BYTES) throw std::out_of_range("Memory::read_word out of range");
    std::memcpy(out_word, mem_.data() + address, WORD_BYTES);
}

void Memory::write_word(uint64_t address, const uint64_t* in_word) {
    if (address + WORD_BYTES > MEM_BYTES) throw std::out_of_range("Memory::write_word out of range");
    std::memcpy(mem_.data() + address, in_word, WORD_BYTES);
}

void Memory::read_bytes(uint64_t address, uint64_t* out_buf, size_t n) const {
    if (address + n > MEM_BYTES) throw std::out_of_range("Memory::read_bytes out of range");
    std::memcpy(out_buf, mem_.data() + address, n);
}