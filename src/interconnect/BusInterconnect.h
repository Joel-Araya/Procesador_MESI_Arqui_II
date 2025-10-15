#ifndef BUS_INTERCONNECT_H
#define BUS_INTERCONNECT_H

#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include "BusTransaction.h"
#include "../utils/ConcurrentQueue.h"
#include "../components/Memory_test.h"
#include "../components/CacheL1_test.h"

class BusInterconnect {
public:
    BusInterconnect(std::vector<CacheL1*>& caches, Memory* memory);
    ~BusInterconnect();

    // Funcion que se ejecuta en el hilo del Bus
    void run();

    // Interfaz para que una CacheL1 envie una peticion al Bus
    void add_request(const BusTransaction& transaction);

private:
    std::thread bus_thread_;
    std::mutex arbit_mutex_;
    int last_granted_pe_ = -1;

    // Cola de peticiones del Bus
    ConcurrentQueue<BusTransaction> request_queue_;

    // Punteros a los otros modulos para invocar Snooping y accesos a Memoria
    std::vector<CacheL1*>& caches_;
    Memory* memory_;

    // Logica de Arbitraje y Proceso MESI
    void arbitrate_and_process();
    void process_transaction(BusTransaction& transaction);

    // Funcion auxiliar para obtener el nombre del comando para prints
    std::string get_command_name(BusCommand cmd) const;
};

#endif // BUS_INTERCONNECT_H
