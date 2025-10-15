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
#include "PE/SharedMemory.hpp"
#include "PE/ProgramLoader.hpp"


// Funcion de prueba del comportamiento de un PE/Cache
void pe_thread_function(int pe_id, BusInterconnect& bus, int num_requests) {
    std::cout << "[PE " << pe_id << "] Iniciando simulador de actividad.\n";
    
    // Simulación de peticiones (e.g., Cache Misses)
    for (int i = 0; i < num_requests; ++i) {
        // Genera una dirección y un comando de bus (simulando un LOAD que falló)
        uint64_t address = 0x100 * pe_id + 0x8 * i;
        BusCommand cmd = (i % 2 == 0) ? BusCommand::BUS_READ : BusCommand::BUS_READ_X;

        BusTransaction transaction(pe_id, cmd, address);
        
        std::cout << "[PE " << pe_id << "] Generando petición: " 
                  << (cmd == BusCommand::BUS_READ ? "BUS_READ" : "BUS_READ_X")
                  << " @ 0x" << std::hex << address << std::dec << "\n";
        
        bus.add_request(transaction);
        
        // Pausa aleatoria para simular latencia de ejecución entre peticiones
        std::this_thread::sleep_for(std::chrono::milliseconds(50 + (pe_id * 10)));
    }
    std::cout << "[PE " << pe_id << "] Actividad finalizada.\n";
}

// Funcion principal de prueba
void test_interconnect_concurrency() {
    std::cout << "\n======================================================\n";
    std::cout << "  PRUEBA DE CONCURRENCIA Y ARBITRAJE DEL INTERCONNECT\n";
    std::cout << "======================================================\n\n";

    // 1. Inicializar componentes Dummy
    Memory memory;
    std::vector<CacheL1*> caches;
    for (int i = 0; i < 4; ++i) {
        caches.push_back(new CacheL1(i, &memory));
    }

    // 2. Inicializar el Bus Interconnect
    BusInterconnect bus(caches, &memory);

    // 3. Crear los hilos (Simulando 4 PEs/Cachés)
    std::vector<std::thread> pe_threads;
    int requests_per_pe = 10; 
    
    // Iniciar el hilo del Bus primero
    std::thread bus_thread(&BusInterconnect::run, &bus);
    
    // Iniciar los hilos de los PEs
    for (int i = 0; i < 4; ++i) {
        pe_threads.emplace_back(pe_thread_function, i, std::ref(bus), requests_per_pe);
    }

    // 4. Esperar a que los PEs terminen de generar peticiones
    for (auto& t : pe_threads) {
        t.join();
    }
    
    std::cout << "\nTodos los PEs han terminado de generar peticiones.\n";
    
    // 5. Dejar que el Bus procese las peticiones restantes
    std::this_thread::sleep_for(std::chrono::seconds(2)); 
    
    // NOTA: En la implementación final, el Bus debe tener un mecanismo de 'stop'
    // (Ej: una variable booleana 'running' que se pone en false).
    std::cout << "El Bus ha tenido 2 segundos para procesar.\n";

    // 6. Limpieza
    // Aquí deberías detener el hilo del bus de forma segura y liberar memoria
    // Para esta prueba simple, solo liberamos las cachés
    // for (auto* c : caches) {
    //     delete c;
    // }
    
    //bus_thread.join(); //  <-- Esto causará un bloqueo si no se implementa el 'stop'
    // en la función run() del Bus.
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


void processor_system(){
        ProcessorSystem system;
    SharedMemory mem(256); // suficiente para nuestras posiciones

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
    test_interconnect_concurrency();
    //std::cout << "\n\n";
    //test_memory();
    //std::cout << "\n\n";
    //processor_system();

    return 0;
}
