# Procesador_MESI_Arqui_II

Simulación básica en C++ de un sistema con 4 Processing Elements (PEs), cada uno con:
- 1 hardware thread (std::thread)
- 8 registros de 64 bits (Reg0 - Reg7)

## Estructura
- `ProcessingElement`: encapsula registros y ejecución del hilo.
- `ProcessorSystem`: agrupa los 4 PEs y facilita su lanzamiento.
- `main.cpp`: ejemplo de uso.

## Compilación
```
make -j
./proc_sim
```

## Ejemplo de salida
```
PE0 Reg0=5 Reg1=50
PE1 Reg0=5 Reg1=51
PE2 Reg0=5 Reg1=52
PE3 Reg0=5 Reg1=53
```

Puedes modificar las funciones que se envían a cada PE para simular instrucciones personalizadas.

