#ifndef BUS_INTERCONNECT_H
#define BUS_INTERCONNECT_H

#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include <atomic>
#include "BusTransaction.h"
#include "../utils/ConcurrentQueue.h"
#include "../components/memory.h"
#include "../components/cacheL1.h"

class BusInterconnect {
public:
    BusInterconnect(std::vector<CacheL1*>& caches, Memory* memory);
    ~BusInterconnect();

    // Funcion que se ejecuta en el hilo del Bus
    void run();

    void stop();

    // Interfaz para que una CacheL1 envie una peticion al Bus
    void add_request(const BusTransaction& transaction);

    // Funcion auxiliar para obtener el nombre del comando para prints
    std::string get_command_name(BusCommand cmd) const;

private:
    std::thread bus_thread_;
    std::mutex arbit_mutex_;
    int last_granted_pe_ = -1;
    std::atomic<bool> stop_flag_ = false;

    // Cola de peticiones del Bus
    ConcurrentQueue<BusTransaction> request_queue_;

    // Punteros a los otros modulos para invocar Snooping y accesos a Memoria
    std::vector<CacheL1*>& caches_;
    Memory* memory_;

    std::atomic<bool> running_;

    // Logica de Arbitraje y Proceso MESI
    void arbitrate_and_process();
    void process_transaction(BusTransaction& transaction);

};

#endif // BUS_INTERCONNECT_H
