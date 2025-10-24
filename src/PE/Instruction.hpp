#pragma once
#include <cstddef>
#include <cstdint>

enum class OpCode { LOAD, STORE, FMUL, FADD, INC, DEC, JNZ, HALT,
    MOVI, ADDI, ADD, LOADR, STORER };

struct Instruction {
    OpCode op;
    int rd{-1};   // destination or source (STORE/STORER use rd as store source)
    int ra{-1};   // source A / address register (LOADR uses ra, STORER uses ra for address)
    int rb{-1};   // source B
    uint64_t addr{0}; // memory address OR immediate (for MOVI/ADDI) OR jump target resolution value
    size_t target{0}; // jump target (instruction index) for JNZ
};
