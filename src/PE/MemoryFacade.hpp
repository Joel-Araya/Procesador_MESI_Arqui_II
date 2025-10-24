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
        bus_->add_request(BusTransaction(pe_id_, BusCommand::BUS_READ, addr));
        uint64_t val = cache_->read(addr);
        std::cout << "[MemoryFacade PE " << pe_id_ << "] Load 64b @ 0x" << std::hex << addr << std::dec << " = " << val << std::endl;
        load_counter_++;
        return val;
    }
    void store(uint64_t addr, uint64_t val) override {
        std::cout << "[MemoryFacade PE " << pe_id_ << "] Store 64b @ 0x" << std::hex << addr << std::dec << " = " << val << std::endl;
        bus_->add_request(BusTransaction(pe_id_, BusCommand::BUS_READ_X, addr));
        cache_->write(addr, val); // nuevo método
        store_counter_++;
    }

    int getLoadCount() const { return load_counter_; }
    int getStoreCount() const { return store_counter_; }
    int getPEId() const { return pe_id_; }

private:
    CacheL1* cache_;
    BusInterconnect* bus_;
    int pe_id_; // Identificador del PE asociado
    int load_counter_ = 0;
    int store_counter_ = 0;
};

#endif // MEMORY_FACADE_HPP