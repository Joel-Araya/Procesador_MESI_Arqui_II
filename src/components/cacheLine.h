#pragma once
#include <cstdint>
#include "../interconnect/BusEnunms.h"
#include <array>

struct CacheLine {
    bool valid;
    bool dirty;
    uint64_t tag;
    MESI_State state;       // de BusEnums.h
    std::array<uint8_t, 32> data;
};

