#ifndef BUS_TRANSACTION_H
#define BUS_TRANSACTION_H

#include "BusEnums.h"

struct BusTransaction {
    int pe_id;
    BusCommand command;
    uint64_t address;

    bool hit_shared;
    bool hit_modified;
    bool data_from_memory;

    // Constructor
    BusTransaction(int id, BusCommand cmd, uint64_t addr) 
        : pe_id(id), command(cmd), address(addr),
          hit_shared(false), hit_modified(false), data_from_memory(false) {}
};

#endif // BUS_TRANSACTION_H
