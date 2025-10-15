#include "BusInterconnect.h"
#include <iostream>
#include <functional>


BusInterconnect::BusInterconnect(std::vector<CacheL1*>& caches, Memory* memory)
    : caches_(caches),
    memory_(memory)
{
    std::cout << "BusInterconnect: Inicializando Interconector con " 
    << caches_.size() << "caches.\n";
    
    // Inicializar el contador de arbitraje para empezar por PE_0
    last_granted_pe_ = 3; 
    std::cout << "🔄 Lógica de Arbitraje: Iniciando Round-Robin. El próximo PE a buscar es PE " 
    << (last_granted_pe_ + 1) % 4 << ".\n"; 

    // Inicializa el hilo del bus
    bus_thread_ = std::thread(&BusInterconnect::run, this);

    std::cout << "Hilo de Arbitraje del Bus inicializado.\n";
}

BusInterconnect::~BusInterconnect(){
    if (bus_thread_.joinable()) {
        // En un entorno de prueba es mejor no llamar join sin un mecanismo de stop para evitar deadlock
        // Aqui se asume que la prueba termina rapido
        // bus_thread_.join();
    }

    std::cout << "BusInterconnect: Hilo de Arbitraje finalizado.\n";
}

void BusInterconnect::add_request(const BusTransaction& transaction) {
    // La CacheL1 llama a esta funcion, la peticion se añade a la cola concurrente
    request_queue_.push(transaction);
}

void BusInterconnect::run()
{
    // El hilo del Bus se ejecuta continuamente 
    while(true) {
        // Logica para pausar el bus si no hay peticiones 
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (!request_queue_.empty()) {
            std::cout << "\n[BUS] Peticiones en cola. Iniciando ciclo de Arbitraje...\n";
            arbitrate_and_process();
        }
    }
}

void BusInterconnect::arbitrate_and_process() {
    // Implementacion simple de Round-Robin

    // Encontrar la proxima peticion con prioridad Round-Robin
    int next_pe_id = (last_granted_pe_ + 1) % 4;

    BusTransaction active_transaction = request_queue_.pop_priority(last_granted_pe_);

    std::cout << "[BUS ARBITRADO] PE " << active_transaction.pe_id 
    << " ha ganado el acceso (Prioridad iniciada en PE " << next_pe_id << ").\n";

    // Actualizar el PE que acaba de ser atendido
    last_granted_pe_ = active_transaction.pe_id;
    std::cout << "   -> Bus bloqueado. Próxima búsqueda Round-Robin iniciará en PE " 
    << (last_granted_pe_ + 1) % 4 << ".\n";

    // Procesar la transaccion MESI
    process_transaction(active_transaction);

    // FALTA IMPLEMENTAR - Enviar la respuesta a la Cache solicitante
}

void BusInterconnect::process_transaction(BusTransaction& transaction) {
    // Difundir la transaccion (Snooping)
    std::cout << "📣 [BUS DIFUSIÓN] Difundiendo " 
    << get_command_name(transaction.command) << " @ 0x" 
    << std::hex << transaction.address << std::dec 
    << " (Solicitado por PE " << transaction.pe_id << ").\n";

    // El Bus llama a una funcion de "snoop" en cada una de las 4 caches
    // Solo las caches observadoras (pe_id != transaction.pe_id) responden con su estado
    for (int i = 0; i < 4; ++i) {
        // Este metodo debe estar definido en la clase CacheL1
        auto snoop_result = caches_[i]->snoop_bus(transaction);

        // Analizar el resultado:
        if (snoop_result.had_modified) {
            std::cout << "   <- Snooping: PE " << i << " tenía el dato en estado MODIFIED (M). REQUIERE WRITE-BACK.\n";
            transaction.hit_modified = true;
        } else if (snoop_result.had_shared) {
            std::cout << "   <- Snooping: PE " << i << " tenía el dato en estado SHARED (S).\n";
            transaction.hit_shared = true;
        } else {
            // Solo si el comando es BusRdX, las cachés con S o E cambian a I.
            if (transaction.command == BusCommand::BUS_READ_X) {
                std::cout << "   <- Snooping: PE " << i << " INVALDADO remotamente.\n";
            }
        }
    }

    // Determinar la fuente de datos y el trafico
    if (transaction.hit_modified) {
        // Dato viene de otra Cache (MESI: Write-Back requerido)
        // Actualizar metricas de trafico
        // La cache que tenia M proveera los datos directamente o escribira a memoria 
        // (Esto requiere coordinar con el rol de Memoria y Cache)
        std::cout << "💾 [RESOLUCIÓN] Datos obtenidos de otra Caché. Memoria no accedida.\n";
        // Lógica de esperar el Write-Back a memoria
    } else {
        // Dato viene de Memoria
        std::cout << "💾 [RESOLUCIÓN] Accediendo a Memoria Principal.\n";
        transaction.data_from_memory = true;
        // Llamar al modulo de memoria para obtener datos
        // memory_->read_block(transaction.address);
    }

    std::cout << "--------------------------------------------------------\n";

    // FALTA IMPLEMENTAR - El bus debe enviar la respuesta final (datos y nuevo estado) 
    // de vuelta a la cache solicitante (transaction.pe_id)
}

std::string BusInterconnect::get_command_name(BusCommand cmd) const {
    switch (cmd) {
        case BusCommand::BUS_READ: return "BusRd (LECTURA)";
        case BusCommand::BUS_READ_X: return "BusRdX (ESCRITURA EXCL.)";
        case BusCommand::INVALIDATE: return "Invalidate";
        case BusCommand::BUS_WRITE: return "BusWr (WRITE-BACK)";
        default: return "NONE/UNKNOWN";
    }
}