#ifndef CONCURRENT_QUEUE_H
#define CONCURRENT_QUEUE_H

#include <deque> // Usaremos deque para permitir find_if y erase
#include <mutex>
#include <condition_variable>
#include <optional>
#include <stdexcept>
#include <algorithm>
#include <utility> // Necesario para std::move
#include "../interconnect/BusTransaction.h" 

template <typename T>
class ConcurrentQueue {
public:
    // 1. Agregar un elemento a la cola (CORRECCIÓN: Usar push_back)
    void push(T value) {
        {
            // Bloquea el mutex antes de modificar la cola
            std::lock_guard<std::mutex> lock(mutex_);
            // CORRECCIÓN: std::deque usa push_back()
            queue_.push_back(std::move(value)); 
        }
        // Notifica a un hilo que esté esperando
        condition_var_.notify_one();
    }

    // 2. Extraer un elemento priorizado para Round-Robin (MÉTODO CLAVE)
    T pop_priority(int last_granted_pe) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Espera de forma segura hasta que la cola no esté vacía
        condition_var_.wait(lock, [this] { return !queue_.empty(); });

        // Lógica Round-Robin: busca el siguiente PE en orden cíclico
        int next_pe_id = (last_granted_pe + 1) % 4; 

        // Bucle que busca la prioridad del PE
        for (int i = 0; i < 4; ++i) {
            int target_pe = (next_pe_id + i) % 4;

            // Busca el primer elemento con el target_pe ID
            auto it = std::find_if(queue_.begin(), queue_.end(), 
                [target_pe](const T& trans) {
                    return trans.pe_id == target_pe;
                });

            if (it != queue_.end()) {
                // Petición encontrada: la extraemos y la retornamos
                T transaction = std::move(*it);
                queue_.erase(it);
                return transaction;
            }
        }
        
        // Fallback: Si no se encuentra un PE prioritario (no debería ocurrir en tu lógica)
        T transaction = std::move(queue_.front());
        queue_.pop_front();
        return transaction;
    }
    
    // 3. Extraer el primer elemento 
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T value = std::move(queue_.front());
        queue_.pop_front();
        return value;
    }

    // 4. Verificar si la cola está vacía (CORRECCIÓN: Remover const de mutex_)
    // Para usar lock_guard con un miembro mutable (mutex_), la función no debe ser const.
    bool empty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    // 5. Tamaño (CORRECCIÓN: Remover const de mutex_)
    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::deque<T> queue_; 
    mutable std::mutex mutex_; // Se mantiene como mutable si quieres funciones const que lo usen
    std::condition_variable condition_var_;
};

#endif // CONCURRENT_QUEUE_H