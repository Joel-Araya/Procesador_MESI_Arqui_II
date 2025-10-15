#pragma once
#include <array>
#include <cstdint>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <mutex>
#include <vector>
#include "Instruction.hpp"

class SharedMemory;

class ProcessingElement {
public:
    static constexpr size_t REG_COUNT = 8; // Reg0 - Reg7

    using ThreadFunc = std::function<void(ProcessingElement&)>;

    ProcessingElement(unsigned id);
    ~ProcessingElement();

    // Non copyable
    ProcessingElement(const ProcessingElement&) = delete;
    ProcessingElement& operator=(const ProcessingElement&) = delete;

    // Movable
    ProcessingElement(ProcessingElement&&) noexcept;
    ProcessingElement& operator=(ProcessingElement&&) noexcept;

    void start(ThreadFunc func);
    void join();

    uint64_t readReg(size_t idx) const;
    void writeReg(size_t idx, uint64_t value);

    unsigned getId() const { return m_id; }

    // Simple helper: add immediate to a register
    void addImm(size_t dstIdx, uint64_t imm);

    void loadProgram(const std::vector<Instruction>& prog);
    void attachMemory(SharedMemory* mem);

private:
    unsigned m_id;
    std::array<uint64_t, REG_COUNT> m_registers{}; // initialize to 0
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    mutable std::mutex m_regMutex; // protect register access
    std::vector<Instruction> m_program;
    size_t m_pc{0};
    SharedMemory* m_mem{nullptr};
};
