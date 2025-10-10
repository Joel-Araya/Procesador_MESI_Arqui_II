#include "ProcessorSystem.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include "Instruction.hpp"
#include "SharedMemory.hpp"
#include <cstring>
#include "ProgramLoader.hpp"

int main() {
    ProcessorSystem system;
    SharedMemory mem(256); // suficiente para nuestras posiciones

    // Inicializar memoria con valores base 10,20,30,40 en direcciones 0,8,16,24
    mem.store(0, 10);
    mem.store(8, 20);
    mem.store(16, 30);
    mem.store(24, 40);

    // Cargar archivos .pec
    std::vector<Instruction> p0 = loadProgramFile("pe0.pec");
    std::vector<Instruction> p1 = loadProgramFile("pe1.pec");
    std::vector<Instruction> p2 = loadProgramFile("pe2.pec");
    std::vector<Instruction> p3 = loadProgramFile("pe3.pec");

    // Adjuntar memoria y cargar programas
    for (size_t i = 0; i < ProcessorSystem::PE_COUNT; ++i) {
        system.getPE(i).attachMemory(&mem);
    }
    system.loadProgram(0, p0);
    system.loadProgram(1, p1);
    system.loadProgram(2, p2);
    system.loadProgram(3, p3);

    // Lanzar todos usando su programa (funcion nula)
    for (size_t i = 0; i < ProcessorSystem::PE_COUNT; ++i) {
        system.getPE(i).start(nullptr);
    }

    system.joinAll();

    std::cout << "Mem[0]  = " << mem.load(0) << " (esperado 10)\n";
    std::cout << "Mem[8]  = " << mem.load(8) << " (esperado 21)\n";
    std::cout << "Mem[16] = " << mem.load(16) << " (esperado 32)\n";
    std::cout << "Mem[24] = " << mem.load(24) << " (esperado 43)\n";

    return 0;
}
