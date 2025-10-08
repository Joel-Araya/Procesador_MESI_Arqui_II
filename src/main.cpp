#include <iostream>
#include "components/memory.h"
#include "components/cacheL1.h"

int main() {
    Memory mem;
    CacheL1 cache0(0, &mem);
    CacheL1 cache1(1, &mem);

    uint64_t addr = 0x00; // mismo bloque

    uint8_t A[8] = {1,2,3,4,5,6,7,8};
    uint8_t out[8];

    // 1) PE0 escribe -> cache0 pone la línea en M
    cache0.write(addr, A);
    std::cout << "cache0 state after write: " << static_cast<int>(cache0.get_line_state(addr)) << " (expect MODIFIED=3)\n";

    // 2) PE1 hace un Read -> Bus broadcast BusRd(addr)
    // Bus debe preguntar a todas las caches (incluye cache0)
    auto r0 = cache0.snoop_bus_rd(addr); // debería indicar had_modified = true y pasar cache0->S
    auto r1 = cache1.snoop_bus_rd(addr); // cache1 no la tiene

    // Determinar fuente de datos: si r0.had_modified -> usar r0.data; else leer de memoria
    std::array<uint8_t,32> block;
    if (r0.had_modified) {
        block = r0.data;
        // (en una implementación completa, el bus sería responsable de escribir a memoria)
    } else {
        mem.read_block(addr, block.data());
    }

    // Ahora bus entrega el bloque a cache1 (others_have = true porque cache0 tenía la línea)
    cache1.load_block_from_bus(addr, block.data(), /*others_have=*/true);

    std::cout << "States after BusRd -> cache0: " << static_cast<int>(cache0.get_line_state(addr))
              << " cache1: " << static_cast<int>(cache1.get_line_state(addr)) << "\n";

    // cache1 lee (local hit)
    cache1.read(addr, out);
    std::cout << "cache1 read: ";
    for (int i=0;i<8;i++) std::cout << (int)out[i] << " ";
    std::cout << "\n";

    // 3) Ahora PE1 quiere escribir -> BusRdX (invalida a otros)
    auto s0 = cache0.snoop_bus_rdx(addr);
    auto s1 = cache1.snoop_bus_rdx(addr); // cache1 will be invalidated by BusRdX as well

    // si alguien devolvió had_modified==true, bus would writeback before granting
    std::array<uint8_t,32> block_for_requester;
    if (s0.had_modified) block_for_requester = s0.data;
    else if (s1.had_modified) block_for_requester = s1.data;
    else mem.read_block(addr, block_for_requester.data());

    // bus grants exclusive ownership to requester (cache1) -> load block from bus with others_have=false
    cache1.load_block_from_bus(addr, block_for_requester.data(), /*others_have=*/false);

    std::cout << "States after BusRdX -> cache0: " << static_cast<int>(cache0.get_line_state(addr))
              << " cache1: " << static_cast<int>(cache1.get_line_state(addr)) << "\n";

    // cache1 writes new data
    uint8_t NEW[8] = {42,43,44,45,46,47,48,49};
    cache1.write(addr, NEW);

    // Show final states & metrics
    cache0.print_cache_lines();
    cache1.print_cache_lines();

    cache0.print_metrics();
    cache1.print_metrics();

    // Finally check memory (if a writeback occurred earlier it would be reflected)
    uint8_t memblk[32];
    mem.read_block(addr, memblk);
    std::cout << "Memory first 8 bytes: ";
    for (int i=0;i<8;i++) std::cout << (int)memblk[i] << " ";
    std::cout << "\n";

    return 0;
}