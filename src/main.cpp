#include <iostream>
#include "components/memory.h"
#include "components/cacheL1.h"

int main() {
    Memory mem;
    CacheL1 cache(0, &mem);

    uint8_t data[8] = {10,20,30,40,50,60,70,80};
    uint8_t buffer[8] = {0};

    // Primer acceso → Miss
    cache.write(0x00, data);
    // Segundo acceso a la misma línea → Hit
    cache.read(0x00, buffer);
    // Otro bloque → Miss
    cache.read(0x40, buffer);
    // Otro del mismo set pero distinto tag → reemplazo
    cache.read(0x140, buffer);

    cache.print_metrics();
    return 0;
}
