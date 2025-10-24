#include "BusInterconnect.h"
#include <iostream>
#include <functional>
#include <cstring>


BusInterconnect::BusInterconnect(std::vector<CacheL1*>& caches, Memory* memory)
    : caches_(caches),
    memory_(memory),
    running_(true)
{
    std::cout << "BusInterconnect: Inicializando Interconector con " 
    << caches_.size() << " caches.\n";
    
    last_granted_pe_ = 3; 
    std::cout << "Lógica de Arbitraje: Iniciando Round-Robin. El próximo PE a buscar es PE " 
    << (last_granted_pe_ + 1) % 4 << ".\n"; 

    bus_thread_ = std::thread(&BusInterconnect::run, this);
    std::cout << "Hilo de Arbitraje del Bus inicializado.\n";
}

BusInterconnect::~BusInterconnect(){
    stop_flag_ = true;
    if (bus_thread_.joinable()) {
        bus_thread_.join();
    }

    std::cout << "BusInterconnect: Hilo de Arbitraje finalizado.\n";
}

void BusInterconnect::stop(){
    running_.store(false);
    if (bus_thread_.joinable()) bus_thread_.join();
}

void BusInterconnect::add_request(const BusTransaction& transaction) {
    request_queue_.push(transaction);
}

void BusInterconnect::run() {
    bool processing = true;
    while(!stop_flag_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!request_queue_.empty()) {
            std::cout << "\n[BUS] Peticiones en cola. Iniciando ciclo de Arbitraje...\n";
            arbitrate_and_process();
            processing = false;
        } else if (!processing) {
            std::cout << "[BUS] No hay más peticiones en cola. Esperando nuevas solicitudes...\n";
            stop_flag_ = true;
        }
    }
}

void BusInterconnect::arbitrate_and_process() {
    int next_pe_id = (last_granted_pe_ + 1) % 4;

    BusTransaction active_transaction = request_queue_.pop_priority(last_granted_pe_);

    std::cout << "[BUS ARBITRADO] PE " << active_transaction.pe_id 
    << " ha ganado el acceso (Prioridad iniciada en PE " << next_pe_id << ").\n";

    last_granted_pe_ = active_transaction.pe_id;
    std::cout << "\t-> Bus bloqueado. Próxima búsqueda Round-Robin iniciará en PE " 
    << (last_granted_pe_ + 1) % 4 << ".\n";

    process_transaction(active_transaction);
}

void BusInterconnect::process_transaction(BusTransaction& transaction) {
    std::array<uint8_t, CacheL1::BLOCK_BYTES> data_block; // ahora 32B
    int data_provider_pe = -1;

    std::cout << "[BUS DIFUSIÓN] Difundiendo " 
    << get_command_name(transaction.command) << " @ 0x" 
    << std::hex << transaction.address << std::dec 
    << " (Solicitado por PE " << transaction.pe_id << ").\n";

    for (int i = 0; i < 4; ++i) {
        if (i == transaction.pe_id) continue;

        CacheL1::BusSnoopResult snoop_result;

        if (transaction.command == BusCommand::BUS_READ) {
            snoop_result = caches_[i]->snoop_bus_rd(transaction.address);
        } else if (transaction.command == BusCommand::BUS_READ_X) {
            snoop_result = caches_[i]->snoop_bus_rdx(transaction.address);
        }

        if (snoop_result.had_modified) {
            std::cout << "\t<- Snooping: PE " << i << " tenía el dato en estado MODIFIED (M). REQUIERE WRITE-BACK.\n";
            if (!transaction.hit_modified) {
                transaction.hit_modified = true;
                data_provider_pe = i;
                std::memcpy(data_block.data(), snoop_result.data.data(), CacheL1::BLOCK_BYTES);
            }
        }
        
        if (snoop_result.had_shared) {
            transaction.hit_shared = true;
            std::cout << "\t<- Snooping: PE " << i << " tenía el dato en estado SHARED/EXCLUSIVE (S/E).\n";
        }
    }

    if (transaction.hit_modified) {
        std::cout << "[RESOLUCIÓN] Datos obtenidos de Caché PE " << data_provider_pe << ".\n";
        
        memory_->write_block(transaction.address, reinterpret_cast<const uint64_t *>(data_block.data()));
        std::cout << "[MEM] Write-back completado a Memoria (32B).\n";
    } else {
        std::cout << "[RESOLUCIÓN] Accediendo a Memoria Principal.\n";

        memory_->read_block(transaction.address, reinterpret_cast<uint64_t *>(data_block.data()));
        transaction.data_from_memory = true;
    }

    bool others_have = transaction.hit_shared || transaction.hit_modified;

    if (transaction.command == BusCommand::BUS_READ_X) {
        others_have = false;
        std::cout << "\t-> Escritura (BusRdX): Garantizando estado EXCLUSIVE para PE " << transaction.pe_id << ".\n";
    } else {
        std::cout << "\t-> Lectura (BusRd): Estado final es " << (others_have ? "SHARED" : "EXCLUSIVE") << ".\n";
    }

    caches_[transaction.pe_id]->load_block_from_bus(
        transaction.address, 
        data_block.data(), 
        others_have
    );

    std::cout << "--------------------------------------------------------\n";
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