#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>
#include <chrono>
#include <thread>
#include <iostream>

class Memory {
public:
    Memory() { /* Constructor stub */ }
    
    // Método que el Bus Interconnect llama para obtener datos
    void read_block(uint64_t address) {
        // Simulación de latencia de acceso a memoria
        std::this_thread::sleep_for(std::chrono::microseconds(500)); 
        // std::cout << "[MEMORIA] Bloque de datos leido en 0x" << std::hex << address << std::dec << ".\n";
    }
    
    // Aquí irían otros métodos: write_block, etc.
};

#endif // MEMORY_H