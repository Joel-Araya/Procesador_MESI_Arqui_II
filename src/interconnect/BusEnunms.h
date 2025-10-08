#ifndef BUS_ENUMS_H
#define BUS_ENUMS_H

#include <cstdint>

// Estados del Protocolo MESI (a nivel de linea de cache)
enum class MESI_State {
    INVALID = 0,
    EXCLUSIVE = 1,
    SHARED = 2,
    MODIFIED = 3
};

// Comando de Transaccion en el Bus
enum class BusCommand {
    BUS_READ = 0,
    BUS_READ_X = 1,
    INVALIDATE = 2,
    BUS_WRITE = 3,
    NONE = 99
};

#endif // BUS_ENUMS_H