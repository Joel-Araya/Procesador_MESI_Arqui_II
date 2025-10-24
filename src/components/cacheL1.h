#ifndef CACHE_L1_H
#define CACHE_L1_H

#include <cstdint>
#include <array>
#include <iostream>
#include "memory.h"
#include "../interconnect/BusEnums.h"
#include "../utils/metrics.h"
#include "cacheLine.h"

class CacheL1 {
public:
    CacheL1(int id, Memory* mem);

    // API simple: leer/escribir 8 bytes (palabra de 64-bit)
    void write(uint64_t address, uint64_t data64); // cambiado
    uint64_t read(uint64_t address);               // cambiado

    // --- MESI / Bus-facing iface (para que el Bus llame) ---
    // Resultado de snooping
    struct BusSnoopResult {
        bool had_modified = false;   // había M -> requiere writeback
        bool had_shared = false;     // había S/E
        std::array<uint8_t,32> data; // si had_modified==true, incluye el bloque que tenía la cache
    };

    // Snooping: cuando el bus difunde un BusRd (Read)
    BusSnoopResult snoop_bus_rd(uint64_t address);

    // Snooping: cuando el bus difunde un BusRdX (Read for Ownership / Write)
    BusSnoopResult snoop_bus_rdx(uint64_t address);

    // El bus entrega un bloque (ya sea traído de memoria o de otra cache)
    // others_have=true si alguna otra cache tenía la línea (entonces es SHARED), false si nadie la tenía (entonces EXCLUSIVE)
    void load_block_from_bus(uint64_t address, const uint8_t* block32, bool others_have);

    // Invalidar línea local (invocado por bus en BusRdX o Invalidate)
    void invalidate_line(uint64_t address);

    // Debug / inspección
    MESI_State get_line_state(uint64_t address) const;
    void print_cache_lines() const;

    void print_metrics() const;

    static constexpr int BLOCK_BYTES = 8;

private:
    int id_;
    Memory* memory_;
    Metrics metrics_;

    static constexpr int SETS = 8;      // 8 sets
    static constexpr int WAYS = 2;      // 2-way

    std::array<std::array<CacheLine, WAYS>, SETS> sets_;

    // helpers de direccionamiento
    inline uint64_t get_index(uint64_t address) const { return (address >> 5) & 0x7; } // bits [5..7]
    inline uint64_t get_tag(uint64_t address) const { return (address >> 8); }        // resto arriba de bit 8
    inline uint64_t get_offset(uint64_t address) const { return address & 0x1F; }      // 5 bits

    CacheLine* find_line(uint64_t index, uint64_t tag);
    CacheLine* select_victim(uint64_t index);

    // Cuando se reemplaza una línea sucia -> write-back a memoria
    void writeback_if_dirty(CacheLine* line, uint64_t index);
};

#endif // CACHE_L1_H
