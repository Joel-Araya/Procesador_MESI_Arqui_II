#pragma once
#include <cstdint>
#include "../interconnect/BusEnunms.h"
#include <array>

struct CacheLine {
    bool valid = false;
    bool dirty = false;
    uint64_t tag = 0;
    MESI_State state = MESI_State::INVALID;
    std::array<uint8_t, 32> data; // bloque de 32 bytes

    CacheLine() { data.fill(0); }


};