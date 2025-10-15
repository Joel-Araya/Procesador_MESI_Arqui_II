#include "cacheL1.h"
#include <cstring>

CacheL1::CacheL1(int id, Memory* mem)
        : id_(id), memory_(mem) {
    // sets_ inicializado por defecto con CacheLine::CacheLine()
}

/* ------------------ helpers básicos ------------------ */

CacheLine* CacheL1::find_line(uint64_t index, uint64_t tag) {
    for (int w = 0; w < WAYS; ++w) {
        CacheLine& ln = sets_[index][w];
        if (ln.valid && ln.tag == tag) return &ln;
    }
    return nullptr;
}

CacheLine* CacheL1::select_victim(uint64_t index) {
    // 1) buscar línea inválida
    for (int w = 0; w < WAYS; ++w) {
        if (!sets_[index][w].valid) return &sets_[index][w];
    }
    // 2) simple victim: way 0 (puedes mejorar con LRU más tarde)
    return &sets_[index][0];
}

void CacheL1::writeback_if_dirty(CacheLine* line, uint64_t index) {
    if (!line) return;
    if (line->valid && line->dirty) {
        uint64_t block_number = line->tag * SETS + index;
        uint64_t block_addr = block_number * BLOCK_BYTES;
        memory_->write_block(block_addr, line->data.data());
        line->dirty = false;
        // tras writeback, la línea puede ser reutilizada
    }
}

/* ------------------ Operaciones CPU-facing ------------------ */

void CacheL1::write(uint64_t address, const uint8_t* data8) {
    uint64_t index = get_index(address);
    uint64_t tag = get_tag(address);
    uint64_t offset = get_offset(address);

    CacheLine* line = find_line(index, tag);
    if (!line) {
        // Miss -> write-allocate: traer bloque de memoria al caché
        metrics_.misses++;
        CacheLine* victim = select_victim(index);

        // Si la víctima está dirty -> write-back
        writeback_if_dirty(victim, index);

        // Cargar bloque nuevo desde memoria
        uint64_t block_addr = ((tag * SETS) + index) * BLOCK_BYTES;
        memory_->read_block(block_addr, victim->data.data());

        // Inicializar metadata
        victim->valid = true;
        victim->dirty = false; // aún no escrito
        victim->tag = tag;
        victim->state = MESI_State::EXCLUSIVE; // localmente asumimos exclusive (nadie más lo tiene todavía)
        line = victim;
    } else {
        metrics_.hits++;
        // si estaba SHARED y vamos a escribir -> necesitaremos exclusividad global (Bus en capa superior)
    }

    // Escribir los 8 bytes dentro del bloque
    std::memcpy(line->data.data() + offset, data8, 8);
    line->dirty = true;
    line->state = MESI_State::MODIFIED;
}

void CacheL1::read(uint64_t address, uint8_t* out8) {
    uint64_t index = get_index(address);
    uint64_t tag = get_tag(address);
    uint64_t offset = get_offset(address);

    CacheLine* line = find_line(index, tag);
    if (!line) {
        // Miss -> traer bloque desde memoria
        metrics_.misses++;
        CacheLine* victim = select_victim(index);

        // Si la víctima está dirty -> write-back antes de sobreescribir
        writeback_if_dirty(victim, index);

        uint64_t block_addr = ((tag * SETS) + index) * BLOCK_BYTES;
        memory_->read_block(block_addr, victim->data.data());

        victim->valid = true;
        victim->dirty = false;
        victim->tag = tag;
        victim->state = MESI_State::EXCLUSIVE; // localmente asumimos exclusive
        line = victim;
    } else {
        metrics_.hits++;
    }

    std::memcpy(out8, line->data.data() + offset, 8);
}

/* --------------- Métodos que usará el Bus (Snooping) ------------- */

/*
 BusRd (read broadcast)
 - Si la cache tiene la línea en M -> debe indicar had_modified=true y pasar a S (y el Bus/Memory coordinará writeback).
 - Si la cache tiene E -> pasar a S, had_shared=true.
 - Si la cache tiene S -> had_shared=true.
 - Si I -> no participa.
*/
CacheL1::BusSnoopResult CacheL1::snoop_bus_rd(uint64_t address) {
    BusSnoopResult res;
    uint64_t index = get_index(address);
    uint64_t tag = get_tag(address);
    CacheLine* line = find_line(index, tag);
    if (!line) return res;

    if (line->state == MESI_State::MODIFIED) {
        // debe suministrar datos (writeback o supply via bus)
        res.had_modified = true;
        std::memcpy(res.data.data(), line->data.data(), BLOCK_BYTES);
        // según MESI, tras BusRd una cache con M pasa a S (y hace writeback)
        line->state = MESI_State::SHARED;
        // la línea sigue válida; dirty se limpia una vez que el bus/mem haga writeback
        line->dirty = false;
    } else if (line->state == MESI_State::EXCLUSIVE) {
        res.had_shared = true;
        line->state = MESI_State::SHARED;
    } else if (line->state == MESI_State::SHARED) {
        res.had_shared = true;
        // permanece SHARED
    }
    return res;
}

/*
 BusRdX (read for ownership / write intent)
 - Todas las caches que tengan la línea en S/E/M deben invalidarla (I).
 - Si alguna tenía M -> had_modified=true y debe hacer writeback.
*/
CacheL1::BusSnoopResult CacheL1::snoop_bus_rdx(uint64_t address) {
    BusSnoopResult res;
    uint64_t index = get_index(address);
    uint64_t tag = get_tag(address);
    CacheLine* line = find_line(index, tag);
    if (!line) return res;

    if (line->state == MESI_State::MODIFIED) {
        res.had_modified = true;
        std::memcpy(res.data.data(), line->data.data(), BLOCK_BYTES);
        // writeback required: we'll mark dirty->false and invalidate
        line->dirty = false;
        line->state = MESI_State::INVALID;
        line->valid = false;
        metrics_.invalidations++;
    } else if (line->state == MESI_State::EXCLUSIVE || line->state == MESI_State::SHARED) {
        res.had_shared = true;
        // invalidate
        line->state = MESI_State::INVALID;
        line->valid = false;
        metrics_.invalidations++;
    }
    return res;
}

/*
 El Bus entrega el bloque resultante a la cache solicitante.
 Si others_have==false => nadie más lo tenía (EXCLUSIVE)
 Si others_have==true  => alguien más lo tenía (SHARED)
*/
void CacheL1::load_block_from_bus(uint64_t address, const uint8_t* block32, bool others_have) {
    uint64_t index = get_index(address);
    uint64_t tag = get_tag(address);

    CacheLine* victim = select_victim(index);
    writeback_if_dirty(victim, index);

    // cargar bloque
    std::memcpy(victim->data.data(), block32, BLOCK_BYTES);
    victim->valid = true;
    victim->dirty = false;
    victim->tag = tag;
    victim->state = others_have ? MESI_State::SHARED : MESI_State::EXCLUSIVE;
}

/*
 Invalidar localmente (invocado por el bus para mantener coherencia)
*/
void CacheL1::invalidate_line(uint64_t address) {
    uint64_t index = get_index(address);
    uint64_t tag = get_tag(address);
    CacheLine* line = find_line(index, tag);
    if (!line) return;
    line->valid = false;
    line->dirty = false;
    line->state = MESI_State::INVALID;
    metrics_.invalidations++;
}

/* ---------------- Debug / inspección ---------------- */

MESI_State CacheL1::get_line_state(uint64_t address) const {
    uint64_t index = (address >> 5) & 0x7;
    uint64_t tag = (address >> 8);
    for (int w=0; w<WAYS; ++w) {
        const CacheLine& ln = sets_[index][w];
        if (ln.valid && ln.tag == tag) return ln.state;
    }
    return MESI_State::INVALID;
}

void CacheL1::print_cache_lines() const {
    std::cout << "Cache" << id_ << " contents:\n";
    for (int i=0;i<SETS;i++) {
        std::cout << " Set " << i << ":\n";
        for (int w=0; w<WAYS; ++w) {
            const CacheLine& ln = sets_[i][w];
            std::cout << "  Way" << w << ": valid=" << ln.valid
                      << " dirty=" << ln.dirty
                      << " tag=" << ln.tag
                      << " state=" << static_cast<int>(ln.state) << "\n";
        }
    }
}

void CacheL1::print_metrics() const {
    metrics_.print(id_);
}