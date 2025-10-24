#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>

// Incluye archivos de Interconnect
#include "interconnect/BusEnums.h"
#include "interconnect/BusTransaction.h"
#include "interconnect/BusInterconnect.h"

#include "components/memory.h"
#include "components/cacheL1.h"

#include "PE/ProcessorSystem.hpp"
#include "PE/Instruction.hpp"
#include "PE/ProgramLoader.hpp"
#include "PE/MemoryFacade.hpp"
#include "PE/SharedMemoryInstance.hpp"


std::string get_mesi_state_name(MESI_State state) {
    switch (state) {
        case MESI_State::MODIFIED: return "MODIFIED (M)";
        case MESI_State::EXCLUSIVE: return "EXCLUSIVE (E)";
        case MESI_State::SHARED: return "SHARED (S)";
        case MESI_State::INVALID: return "INVALID (I)";
        default: return "UNKNOWN";
    }
}

// Funcion de prueba del comportamiento de un PE/Cache
void pe_activity(int pe_id, BusInterconnect& bus, uint64_t address_base, int num_reads, int num_writes) {
    std::cout << "--> [PE " << pe_id << "] Iniciando actividad: " << num_reads << " lecturas, " << num_writes << " escrituras.\n";
    
    // 1. Lecturas (BusRd)
    for (int i = 0; i < num_reads; ++i) {
        uint64_t address = address_base + (i * 8); 
        BusTransaction transaction(pe_id, BusCommand::BUS_READ, address);
        
        std::cout << "[PE " << pe_id << "] REQ: BusRd @ 0x" << std::hex << address << std::dec << "\n";
        bus.add_request(transaction);
        std::this_thread::sleep_for(std::chrono::milliseconds(50 + (pe_id * 5))); // Pausa para simular latencia
    }

    // 2. Escrituras (BusRdX)
    for (int i = 0; i < num_writes; ++i) {
        uint64_t address = address_base + 0x400 + (i * 8); // Offset para direcciones diferentes
        BusTransaction transaction(pe_id, BusCommand::BUS_READ_X, address);
        
        std::cout << "[PE " << pe_id << "] REQ: BusRdX @ 0x" << std::hex << address << std::dec << "\n";
        bus.add_request(transaction);
        std::this_thread::sleep_for(std::chrono::milliseconds(100 + (pe_id * 10))); // Pausa para simular latencia
    }
    
    std::cout << "<-- [PE " << pe_id << "] Actividad finalizada.\n";
}

void test_interconnect_full_mesi() {
    std::cout << "\n======================================================\n";
    std::cout << "  PRUEBA MESI COMPLETA Y ARBITRAJE DEL INTERCONNECT\n";
    std::cout << "======================================================\n\n";

    // 1. Inicializar Componentes (Memoria y 4 Cachés)
    Memory memory;
    std::vector<CacheL1*> caches;
    for (int i = 0; i < 4; ++i) {
        // Inicializar cada Cache L1 con su ID y puntero a la Memoria
        caches.push_back(new CacheL1(i, &memory));
    }

    // 2. Inicializar el Bus Interconnect
    BusInterconnect bus(caches, &memory, false);
    
    // 3. Crear hilos de PEs y asignar cargas
    std::vector<std::thread> pe_threads;

    // SCENARIO 1: Lecturas Compartidas y Round-Robin
    // PE 0 y PE 2 piden el mismo bloque (0x1000) -> Ambos deben terminar en estado S.
    // PE 1 y PE 3 piden bloques distintos.
    pe_threads.emplace_back(pe_activity, 0, std::ref(bus), 0x100, 2, 0); 
    pe_threads.emplace_back(pe_activity, 1, std::ref(bus), 0x200, 1, 0); 
    pe_threads.emplace_back(pe_activity, 2, std::ref(bus), 0x100, 1, 0); 
    pe_threads.emplace_back(pe_activity, 3, std::ref(bus), 0x300, 1, 0); 
    
    // SCENARIO 2: Conflicto de Escritura (Write-Miss)
    // Todos los PEs piden exclusividad sobre direcciones cercanas
    pe_threads.emplace_back(pe_activity, 0, std::ref(bus), 0x400, 0, 1);
    pe_threads.emplace_back(pe_activity, 1, std::ref(bus), 0x400, 0, 1);
    
    // 4. Esperar a que los PEs terminen de generar peticiones
    for (auto& t : pe_threads) {
        t.join();
    }
    
    std::cout << "\n[MAIN] Todos los PEs han terminado de generar peticiones. Esperando Arbitraje final...\n";
    
    // 5. Dejar que el Bus procese las peticiones restantes (da tiempo para que el Bus vacíe la cola)
    std::this_thread::sleep_for(std::chrono::seconds(3)); 
    
    // 6. Verificación Final de Estados MESI (Debug)
    std::cout << "\n======================================================\n";
    std::cout << "  VERIFICACIÓN FINAL DE ESTADOS DE CACHÉ\n";
    std::cout << "======================================================\n";
    
    // --- Bloque 0x100 (Lecturas compartidas) ---
    // EXPECTED: PE 0 (S), PE 2 (S), PE 1 (I), PE 3 (I)
    std::cout << "Estado de 0x100:\n";
    for (int i = 0; i < 4; ++i) {
        MESI_State state = caches[i]->get_line_state(0x100); // Se usa la dirección base
        
        bool expected_shared = (i == 0 || i == 2); 
        // PE 0 y PE 2 realizaron BusRd de un bloque compartido, deben terminar en S
        MESI_State expected_state = expected_shared ? MESI_State::SHARED : MESI_State::INVALID;

        bool success = (state == expected_state);

        std::cout << " - PE " << i << ": " << get_mesi_state_name(state) 
                  << " (" << (success ? "OK" : "FALLÓ") << ". Esperado: " 
                  << get_mesi_state_name(expected_state) << ")\n";
    }

    // --- Bloque 0x800 (Escritura exclusiva final) ---
    // EXPECTED: PE 1 (E/M), PE 0, 2, 3 (I)
    std::cout << "\nEstado de 0x800:\n";
    for (int i = 0; i < 4; ++i) {
        MESI_State state = caches[i]->get_line_state(0x800); // Se usa la dirección base
        
        // El último en escribir fue PE 1 (BusRdX @ 0x800)
        MESI_State expected_state = (i == 1) ? MESI_State::EXCLUSIVE : MESI_State::INVALID; 
        
        bool success = (state == expected_state || (i==1 && state == MESI_State::MODIFIED));
        
        std::cout << " - PE " << i << ": " << get_mesi_state_name(state) 
                  << " (" << (success ? "OK" : "FALLÓ") << ". Esperado: " 
                  << get_mesi_state_name(expected_state) << ")\n";
    }

    // 7. Limpieza
    for (auto* c : caches) {
        delete c;
    }
    
    // El destructor del Bus se llama automáticamente, pero la unión del hilo debe ser gestionada
    // correctamente en el código final para evitar bloqueos.
}

void test_memory(){
    Memory mem;
    CacheL1 cache0(0, &mem);
    CacheL1 cache1(1, &mem);

    uint64_t addr = 0x00; // mismo bloque

    uint64_t A = 0x0807060504030201ULL; // empaquetar los 8 bytes
    uint64_t out64 = 0;

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
        mem.read_block(addr, reinterpret_cast<uint64_t *>(block.data()));
    }

    // Ahora bus entrega el bloque a cache1 (others_have = true porque cache0 tenía la línea)
    cache1.load_block_from_bus(addr, block.data(), /*others_have=*/true); // pasa a SHARED

    std::cout << "States after BusRd -> cache0: " << static_cast<int>(cache0.get_line_state(addr))
              << " cache1: " << static_cast<int>(cache1.get_line_state(addr)) << "\n";

    // cache1 lee (local hit)
    out64 = cache1.read(addr);
    std::cout << "cache1 read (64b): 0x" << std::hex << out64 << std::dec << "\n";

    // 3) Ahora PE1 quiere escribir -> BusRdX (invalida a otros)
    auto s0 = cache0.snoop_bus_rdx(addr);
    auto s1 = cache1.snoop_bus_rdx(addr); // cache1 will be invalidated by BusRdX as well

    // si alguien devolvió had_modified==true, bus would writeback before granting
    std::array<uint8_t,32> block_for_requester;
    if (s0.had_modified) block_for_requester = s0.data;
    else if (s1.had_modified) block_for_requester = s1.data;
    else mem.read_block(addr, reinterpret_cast<uint64_t *>(block_for_requester.data()));

    // bus grants exclusive ownership to requester (cache1) -> load block from bus with others_have=false
    cache1.load_block_from_bus(addr, block_for_requester.data(), /*others_have=*/false);

    std::cout << "States after BusRdX -> cache0: " << static_cast<int>(cache0.get_line_state(addr))
              << " cache1: " << static_cast<int>(cache1.get_line_state(addr)) << "\n";

    // cache1 writes new data
    uint64_t NEW = 0x31302F2E2D2C2B2AULL; // nuevos bytes 42..49 en ASCII aproximado
    cache1.write(addr, NEW);

    // Show final states & metrics
    cache0.print_cache_lines();
    cache1.print_cache_lines();

    cache0.print_metrics();
    cache1.print_metrics();

    // Finally check memory (if a writeback occurred earlier it would be reflected)
    uint64_t memblk_first8 = 0;
    mem.read_block(addr, &memblk_first8);
    std::cout << "Memory first 8 bytes (raw 64b): 0x" << std::hex << memblk_first8 << std::dec << "\n";
}




void processor_system_with_memory_facade(){
    ProcessorSystem system;
    Memory memory;
    
    std::vector<CacheL1*> caches;
    for (int i = 0; i < 4; ++i) {
        caches.push_back(new CacheL1(i, &memory));
    }

    BusInterconnect bus(caches, &memory, false);

    std::vector<MemoryFacade*> memory_facades;
    for (int i = 0; i < 4; ++i) {
        memory_facades.push_back(new MemoryFacade(caches[i], &bus, i));
    }

    // Inicializar memoria con valores base 10,20,30,40 en direcciones 0,32,64,96
    memory.write_block(0, (new uint64_t[1]{10}));
    memory.write_block(32, (new uint64_t[1]{20}));
    memory.write_block(64, (new uint64_t[1]{30}));
    memory.write_block(96, (new uint64_t[1]{40}));

    uint64_t data = 0;
    // std::cout << "Initial memory contents:\n";
    for (size_t j = 0; j < 4; ++j) {
        memory.read_block(j * 32, &data);
        std::cout << "Mem[" << j * 32 << "] = " << static_cast<int>(data) << "\n";
    }

    // Cargar archivos .pec
    std::vector<Instruction> p0 = loadProgramFile("pe0.pec");
    std::vector<Instruction> p1 = loadProgramFile("pe1.pec");
    std::vector<Instruction> p2 = loadProgramFile("pe2.pec");
    std::vector<Instruction> p3 = loadProgramFile("pe3.pec");

    // // Adjuntar memoria y cargar programas
    for (size_t i = 0; i < ProcessorSystem::PE_COUNT; ++i) {
        system.getPE(i).attachMemory(memory_facades[i]);
    }
    system.loadProgram(0, p0);
    system.loadProgram(1, p1);
    system.loadProgram(2, p2);
    system.loadProgram(3, p3);

    // Lanzar todos usando su programa (funcion nula)
    for (size_t i = 0; i < ProcessorSystem::PE_COUNT; ++i) {
        system.getPE(i).start(nullptr);
    }

    system.joinAll();

    std::cout << "Final memory contents:\n";
    for (size_t j = 0; j < 4; ++j) {
        memory.read_block(j * 32, &data);
        std::cout << "Mem[" << j * 32 << "] = " << static_cast<int>(data) << "\n";
    }
}


void processor_system(){
        ProcessorSystem system;
    SharedMemoryInstance mem(256); // suficiente para nuestras posiciones

    // Inicializar memoria con valores base 10,20,30,40 en direcciones 0,8,16,24
    mem.store(0, 10);
    mem.store(8, 20);
    mem.store(16, 30);
    mem.store(24, 40);

    // Cargar archivos .pec
    std::vector<Instruction> p0 = loadProgramFile("pe0.pec");
    std::vector<Instruction> p1 = loadProgramFile("pe1.pec");
    std::vector<Instruction> p2 = loadProgramFile("pe2.pec");
    std::vector<Instruction> p3 = loadProgramFile("pe3.pec");

    // Adjuntar memoria y cargar programas
    for (size_t i = 0; i < ProcessorSystem::PE_COUNT; ++i) {
        system.getPE(i).attachMemory(&mem);
    }
    system.loadProgram(0, p0);
    system.loadProgram(1, p1);
    system.loadProgram(2, p2);
    system.loadProgram(3, p3);

    // Lanzar todos usando su programa (funcion nula)
    for (size_t i = 0; i < ProcessorSystem::PE_COUNT; ++i) {
        system.getPE(i).start(nullptr);
    }

    system.joinAll();

    std::cout << "Mem[0]  = " << mem.load(0) << " (esperado 10)\n";
    std::cout << "Mem[8]  = " << mem.load(8) << " (esperado 21)\n";
    std::cout << "Mem[16] = " << mem.load(16) << " (esperado 32)\n";
    std::cout << "Mem[24] = " << mem.load(24) << " (esperado 43)\n";
}

// Nueva función: prueba de producto punto distribuido en 4 PEs
void processor_system_dot_product(bool debug = false) {
    std::cout << "==== Dot Product distribuido ====" << std::endl;
    std::cout << "Inicializando sistema con Memoria, Cachés y Bus..." << std::endl;
    ProcessorSystem system(debug);
    Memory memory; // memoria compartida detrás de cachés

    std::vector<CacheL1*> caches;
    for (int i = 0; i < 4; ++i) caches.push_back(new CacheL1(i, &memory));
    BusInterconnect bus(caches, &memory, debug);

    std::vector<MemoryFacade*> facades;
    for (int i = 0; i < 4; ++i) facades.push_back(new MemoryFacade(caches[i], &bus, i));

    for (int blk = 0; blk < 16; ++blk) {
        double aVal = static_cast<double>(blk + 1);
        double bVal = static_cast<double>(2 * (blk + 1));
        uint64_t aBits; std::memcpy(&aBits, &aVal, 8);
        uint64_t bBits; std::memcpy(&bBits, &bVal, 8);
        const int baseArrB = 512;
        memory.write_word(blk * 32, &aBits); // usar write_word para una palabra
        memory.write_word(baseArrB + blk * 32, &bBits);
    }
    uint64_t zero = 0;
    memory.write_word(1024, &zero);
    memory.write_word(1056, &zero);
    memory.write_word(1088, &zero);
    memory.write_word(1120, &zero);

    uint64_t data = 0;
    
    
    for (size_t j = 0; j < 4; ++j) {
        memory.read_block(j * 32, &data);
        double a; std::memcpy(&a, &data, sizeof(uint64_t));
        std::cout << "Mem[" << j * 32 << "] = " << a << "\n";
    }

    std::vector<Instruction> p0 = loadProgramFile("pe0.pec");
    std::vector<Instruction> p1 = loadProgramFile("pe1.pec");
    std::vector<Instruction> p2 = loadProgramFile("pe2.pec");
    std::vector<Instruction> p3 = loadProgramFile("pe3.pec");

    for (size_t i = 0; i < ProcessorSystem::PE_COUNT; ++i) system.getPE(i).attachMemory(facades[i]);
    system.loadProgram(0, p0); system.loadProgram(1, p1); system.loadProgram(2, p2); system.loadProgram(3, p3);

    for (size_t i = 0; i < ProcessorSystem::PE_COUNT; ++i) system.getPE(i).start(nullptr);
    system.joinAll();
    std::cout << "Todos los PEs han terminado la ejecución.\n";
    bus.stop();

    // flush caches antes de leer resultados
    for (auto* c : caches) c->flush();

    for (auto* c : caches) {
        c->print_metrics();
        c->print_cache_lines();
    }

    for (size_t j = 0; j < 4; ++j) {
        memory.read_word(j * 32 + 1024, &data);
        double a; std::memcpy(&a, &data, sizeof(uint64_t));
        std::cout << "Partials[" << j * 32 + 1024 << "] = " << a << "\n";
    }
    for (auto* f : facades) delete f; for (auto* c : caches) delete c;

}

void processor_system_dot_product_shared() {
    std::cout << "==== Dot Product (SharedMemoryInstance) ====\n";
    ProcessorSystem system;
    SharedMemoryInstance mem(2048); // suficiente para direccionar hasta 280

    // Inicializar vectores A (base 0) y B (base 128) con 16 doubles
    for (int i = 0; i < 16; ++i) {
        double aVal = static_cast<double>(i + 1);
        double bVal = static_cast<double>(2 * (i + 1));
        uint64_t aBits; std::memcpy(&aBits, &aVal, 8);
        uint64_t bBits; std::memcpy(&bBits, &bVal, 8);
        mem.store(i * 32, aBits);            // A[i]
        mem.store(512 + i * 32, bBits);      // B[i]
    }
    // Inicializar acumuladores parciales (direcciones 256,264,272,280)
    for (int pe = 0; pe < 4; ++pe) mem.store(1024 + pe * 32, 0);

    // Cargar programas
    std::vector<Instruction> p0 = loadProgramFile("pe0.pec");
    std::vector<Instruction> p1 = loadProgramFile("pe1.pec");
    std::vector<Instruction> p2 = loadProgramFile("pe2.pec");
    std::vector<Instruction> p3 = loadProgramFile("pe3.pec");

    // Adjuntar memoria
    for (size_t i = 0; i < ProcessorSystem::PE_COUNT; ++i) system.getPE(i).attachMemory(&mem);
    system.loadProgram(0, p0); system.loadProgram(1, p1); system.loadProgram(2, p2); system.loadProgram(3, p3);

    // Ejecutar
    for (size_t i = 0; i < ProcessorSystem::PE_COUNT; ++i) system.getPE(i).start(nullptr);
    system.joinAll();

    // Leer parciales
    double partials[4];
    for (int pe = 0; pe < 4; ++pe) {
        uint64_t raw = mem.load(1024 + pe * 32);
        std::memcpy(&partials[pe], &raw, 8);
    }
    double finalDot = partials[0] + partials[1] + partials[2] + partials[3];

    std::cout << "Parciales (SharedMemory): ";
    for (double p : partials) std::cout << p << " ";
    std::cout << "\nDot final = " << finalDot << "\n";
}

int main(int argc, char* argv[]) {
    // processor_system_dot_product_shared();
    if (argc > 1 && std::string(argv[1]) == "--debug") {
        processor_system_dot_product(true);
    } else {
        std::cout << "PRUEBA PRODUCTO PUNTO DISTRIBUIDO EN 4 PEs CON CACHÉS Y BUS INTERCONNECT" << std::endl << std::flush;
        processor_system_dot_product();
    }
    // test_interconnect_full_mesi();
    //processor_system_dot_product_shared();
    //std::cout << "\n";
    //processor_system_dot_product();
    //test_interconnect_full_mesi();
    // std::cout << "\n\n";
    // test_memory();
    // std::cout << "\n\n";
    // processor_system();
    // std::cout << "\n\n";
    // processor_system_with_memory_facade();
    return 0;
}
