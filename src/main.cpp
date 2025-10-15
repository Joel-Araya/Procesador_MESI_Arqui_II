#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

// Incluye archivos de Components
#include "components/CacheL1_test.h"
#include "components/Memory_test.h"
// Incluye archivos de Interconnect
#include "interconnect/BusEnums.h"
#include "interconnect/BusTransaction.h"
#include "interconnect/BusInterconnect.h"

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
        caches.push_back(new CacheL1(i));
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
    for (auto* c : caches) {
        delete c;
    }
    
    // bus_thread.join()  <-- Esto causará un bloqueo si no se implementa el 'stop'
    // en la función run() del Bus.
}

int main(int argc, char* argv[]) {
    test_interconnect_concurrency();
    return 0;
}
