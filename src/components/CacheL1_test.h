// src/components/CacheL1_test.h

#ifndef CACHE_L1_TEST_H
#define CACHE_L1_TEST_H

#include <cstdint>
#include <chrono>
#include <thread>
#include "../interconnect/BusTransaction.h" // Necesaria para el argumento BusTransaction

class CacheL1_test { 
public:
    int id;
    
    // La estructura de resultados que el Bus espera de la caché
    struct SnoopResult { 
        bool had_modified = false; 
        bool had_shared = false; 
    };

    CacheL1_test(int i) : id(i) {}

    // ⭐️ FUNCIÓN CRÍTICA: La interfaz para el Bus (Snooping) ⭐️
    SnoopResult snoop_bus(const BusTransaction& transaction) {
        // Lógica de prueba simulada (la misma que estaba en main.cpp)
        std::this_thread::sleep_for(std::chrono::microseconds(10)); 
        
        SnoopResult result;
        if (transaction.address % 2 == 0) {
            result.had_shared = true;
        }
        if (transaction.address % 2 != 0 && id == 1 && transaction.pe_id != 1) {
             result.had_modified = true;
        }
        return result; 
    }

    // Método que el Bus llama para dar los datos al PE solicitante
    void receive_response(const BusTransaction& transaction) {
        // Stub: En la versión final, esto actualiza la línea de caché con los datos recibidos.
    }
};

#endif // CACHE_L1_TEST_H