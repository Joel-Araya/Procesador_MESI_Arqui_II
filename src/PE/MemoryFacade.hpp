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
        // Primero solicitar en bus (intención de lectura)
        bus_->add_request(BusTransaction(pe_id_, BusCommand::BUS_READ, addr));
        uint8_t temp_ = 0;
        cache_->read(addr, &temp_);
        std::cout << "[MemoryFacade PE " << pe_id_ << "] Load from address 0x" << std::hex << addr << std::dec << " = " << static_cast<int>(temp_) << std::endl;

        return static_cast<uint64_t>(temp_);
    }
    void store(uint64_t addr, uint64_t val) override {
        // Solicitar intención de escritura
        std::cout << "[MemoryFacade PE " << pe_id_ << "] Store to address 0x" << std::hex << addr << std::dec << " = " << static_cast<int>(val) << std::endl;
        bus_->add_request(BusTransaction(pe_id_, BusCommand::BUS_READ_X, addr));
        cache_->write(addr, reinterpret_cast<const uint8_t*>(&val));
    }

private:
    CacheL1* cache_;
    BusInterconnect* bus_;
    int pe_id_; // Identificador del PE asociado
};

#endif // MEMORY_FACADE_HPP