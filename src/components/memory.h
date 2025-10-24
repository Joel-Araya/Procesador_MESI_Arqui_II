#ifndef MEMORY_H
#define MEMORY_H

#include <array>
#include <cstdint>
#include <iostream>

class Memory {
public:
    static const int MEM_WORDS = 512;         // 512 palabras de 64 bits
    static const int WORD_BYTES = 8;          // 8 bytes por palabra
    static const int MEM_BYTES = MEM_WORDS * WORD_BYTES;
    static const int BLOCK_BYTES = 32;        // tamaño de bloque de la caché

    Memory();

    // Lee un bloque de 32 bytes alineado en 'address' (address puede ser cualquier byte; se alinea internamente)
    void read_block(uint64_t address, uint64_t* out_block) const;

    // Escribe un bloque de 32 bytes alineado en 'address' (address puede ser cualquier byte; se alinea internamente)
    void write_block(uint64_t address, const uint64_t* in_block);

    // Lectura directa de bytes (para pruebas)
    void read_bytes(uint64_t address, uint64_t* out_buf, size_t n) const;

private:
    std::array<uint8_t, MEM_BYTES> mem_ = std::array<uint8_t, MEM_BYTES>{0};
    uint64_t align_addr(uint64_t address) const;
};

#endif // MEMORY_H
