#ifndef CACHE_L1_H
#define CACHE_L1_H

#include <cstdint>
#include <array>
#include <iostream>
#include "memory.h"
#include "../interconnect/BusEnunms.h"
#include "../utils/metrics.h"
#include "cacheLine.h"

class CacheL1 {
public:
    CacheL1(int id, Memory* mem);

    void write(uint64_t address, const uint8_t* data);
    void read(uint64_t address, uint8_t* buffer);
    void print_metrics() const;



private:
    int id_;
    Memory* memory_;
    Metrics metrics_;  // usa tu versión del struct Metrics

    // 8 sets × 2 ways
    std::array<std::array<CacheLine, 2>, 8> sets_;

    // Funciones auxiliares para dividir direcciones
    uint64_t get_index(uint64_t address) const { return (address >> 5) & 0x7; }  // 3 bits → 8 sets
    uint64_t get_tag(uint64_t address) const { return (address >> 8); }          // resto
    uint64_t get_offset(uint64_t address) const { return address & 0x1F; }


    // Búsqueda de línea
    CacheLine* find_line(uint64_t index, uint64_t tag);
    CacheLine* select_victim(uint64_t index);
};

#endif
