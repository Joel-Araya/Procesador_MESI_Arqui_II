# Procesador_MESI_Arqui_II

Simulación básica en C++ de un sistema con 4 Processing Elements (PEs), cada uno con:
- 1 hardware thread (std::thread)
- 8 registros de 64 bits (Reg0 - Reg7)
- 1 caché asociativo de 2 vias
- Memoria con protocolo MESI


## Compilación
Para unicamente compilar el programa puede usar:
```
make all
```

Para ejecutar el código utilice:
```
make run
```

Para ejecutar el programa en modo debug (stepping):
```
make debug
```

## Ejemplo de salida
```
Partials[1024] = 60
Partials[1056] = 348
Partials[1088] = 892
Partials[1120] = 1692
```

Puedes modificar las funciones que se envían a cada PE para simular instrucciones personalizadas.

