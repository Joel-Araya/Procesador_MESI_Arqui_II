#ifndef MEMORY_FACADE_HPP
#define MEMORY_FACADE_HPP

#include "../components/cacheL1.h"
#include "../interconnect/BusInterconnect.h"
#include "SharedMemory.hpp"

class MemoryFacade : public SharedMemory {
public:
    MemoryFacade(CacheL1* cache, BusInterconnect* bus, int pe_id)
        : cache_(cache), bus_(bus), pe_id_(pe_id) {}
    ~MemoryFacade() = default;

    uint64_t load(uint64_t addr) override{
        uint64_t temp_;
        cache_->read(addr, reinterpret_cast<uint8_t*>(&temp_)); // TODO: reinterpret cast must be eliminated
        bus_->add_request(BusTransaction(pe_id_, BusCommand::BUS_READ, addr));
        return temp_;
    }
    void store(uint64_t addr, uint64_t val) override {
        cache_->write(addr, reinterpret_cast<const uint8_t*>(&val)); // TODO: reinterpret cast must be eliminated
        bus_->add_request(BusTransaction(pe_id_, BusCommand::BUS_WRITE, addr));
    }

private:
    CacheL1* cache_;
    BusInterconnect* bus_;
    int pe_id_; // Identificador del PE asociado
};

#endif // MEMORY_FACADE_HPP