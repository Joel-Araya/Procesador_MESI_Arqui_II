#include "cacheL1.h"
#include <cstring>

CacheL1::CacheL1(int id, Memory* mem)
        : id_(id), memory_(mem) {}

// Busca una línea válida en el set
CacheLine* CacheL1::find_line(uint64_t index, uint64_t tag) {
    for (auto& line : sets_[index]) {
        if (line.valid && line.tag == tag)
            return &line;
    }
    return nullptr;
}

// Selecciona una línea víctima (simple: la primera inválida o la [0])
CacheLine* CacheL1::select_victim(uint64_t index) {
    for (auto& line : sets_[index]) {
        if (!line.valid)
            return &line;
    }
    return &sets_[index][0]; // política sencilla, luego se puede hacer LRU
}

void CacheL1::write(uint64_t address, const uint8_t* data) {
    uint64_t index = get_index(address);
    uint64_t tag = get_tag(address);
    uint64_t offset = get_offset(address);

    CacheLine* line = find_line(index, tag);
    if (!line) {
        // Miss: write-allocate
        metrics_.misses++;
        line = select_victim(index);
        line->valid = true;
        line->dirty = true;
        line->tag = tag;
        memory_->read_block(address - offset, line->data.data());
    } else {
        metrics_.hits++;
    }

    // Escribir los datos (simula solo 8 bytes)
    std::memcpy(line->data.data() + offset, data, 8);
    line->dirty = true;
}

void CacheL1::read(uint64_t address, uint8_t* buffer) {
    uint64_t index = get_index(address);
    uint64_t tag = get_tag(address);
    uint64_t offset = get_offset(address);

    CacheLine* line = find_line(index, tag);
    if (!line) {
        // Miss
        metrics_.misses++;
        line = select_victim(index);
        line->valid = true;
        line->dirty = false;
        line->tag = tag;
        memory_->read_block(address - offset, line->data.data());
    } else {
        metrics_.hits++;
    }

    std::memcpy(buffer, line->data.data() + offset, 8);
}

void CacheL1::print_metrics() const {
    metrics_.print(id_);
}
