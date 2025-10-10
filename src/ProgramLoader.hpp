#pragma once
#include <vector>
#include <string>
#include "Instruction.hpp"

// Carga un archivo .pec y lo convierte en vector<Instruction>
// Formato soportado:
//  LABEL:
//  LOAD Rn, addr
//  STORE Rn, addr
//  INC Rn
//  DEC Rn
//  FADD Rd, Ra, Rb
//  FMUL Rd, Ra, Rb
//  JNZ label   (usa R7 como condición)
//  HALT
//  ; comentarios con ; o #
std::vector<Instruction> loadProgramFile(const std::string& path);
