#pragma once
#include <cstddef>
#include <cstdint>

enum class OpCode { LOAD, STORE, FMUL, FADD, INC, DEC, JNZ, HALT };

struct Instruction {
    OpCode op;
    int rd{-1};   // destination register
    int ra{-1};   // source A
    int rb{-1};   // source B
    uint64_t addr{0}; // memory address / immediate
    size_t target{0}; // jump target (instruction index)
};
