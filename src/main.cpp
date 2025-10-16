#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>

// Incluye archivos de Components
#include "components/CacheL1_test.h"
#include "components/Memory_test.h"

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
    BusInterconnect bus(caches, &memory);
    
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
    
    // Verificar el estado de la línea 0x1000 (debe estar en S para PE 0 y PE 2)
    std::cout << "Estado de 0x100:\n";
    for (int i = 0; i < 4; ++i) {
        MESI_State state = caches[i]->get_line_state(0x100);
        std::cout << " - PE " << i << ": " << bus.get_command_name((BusCommand)state) << " (" << (state == MESI_State::SHARED ? "OK" : "FAIL") << ")\n";
    }

    // Verificar el estado de la línea 0x4000 (el último PE que escribió debe tener M)
    // El orden de arbitraje determinará quién tiene M.
    std::cout << "\nEstado de 0x400:\n";
    for (int i = 0; i < 4; ++i) {
        MESI_State state = caches[i]->get_line_state(0x400);
        std::cout << " - PE " << i << ": " << bus.get_command_name((BusCommand)state) << "\n";
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
    cache1.load_block_from_bus(addr, block.data(), /*others_have=*/true); // pasa a SHARED

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
}


void processor_system_with_memory_facade(){
    ProcessorSystem system;
    Memory memory;
    
    std::vector<CacheL1*> caches;
    for (int i = 0; i < 4; ++i) {
        caches.push_back(new CacheL1(i, &memory));
    }

    BusInterconnect bus(caches, &memory);

    std::vector<MemoryFacade*> memory_facades;
    for (int i = 0; i < 4; ++i) {
        memory_facades.push_back(new MemoryFacade(caches[i], &bus, i));
    }

    // Inicializar memoria con valores base 10,20,30,40 en direcciones 0,32,64,96
    memory.write_block(0, (new uint8_t[1]{10}));
    memory.write_block(32, (new uint8_t[1]{20}));
    memory.write_block(64, (new uint8_t[1]{30}));
    memory.write_block(96, (new uint8_t[1]{40}));

    uint8_t data = 0;
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

int main(int argc, char* argv[]) {
    // test_interconnect_full_mesi();
    //std::cout << "\n\n";
    //test_memory();
    //std::cout << "\n\n";
    // processor_system();
    //std::cout << "\n\n";
    processor_system_with_memory_facade();
    return 0;
}
