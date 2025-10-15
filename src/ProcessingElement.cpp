#include "ProcessingElement.hpp"
#include <stdexcept>
#include "Instruction.hpp"
#include "SharedMemory.hpp"
#include <cstring>

ProcessingElement::ProcessingElement(unsigned id) : m_id(id) {
    m_registers.fill(0);
}

ProcessingElement::~ProcessingElement() {
    if (m_thread.joinable()) {
        m_running = false;
        m_thread.join();
    }
}

ProcessingElement::ProcessingElement(ProcessingElement&& other) noexcept : m_id(other.m_id) {
    std::scoped_lock lock(other.m_regMutex);
    m_registers = other.m_registers;
    m_running = other.m_running.load();
    if (other.m_thread.joinable()) {
        m_thread = std::move(other.m_thread);
    }
}

ProcessingElement& ProcessingElement::operator=(ProcessingElement&& other) noexcept {
    if (this != &other) {
        if (m_thread.joinable()) m_thread.join();
        std::scoped_lock lock(other.m_regMutex, m_regMutex);
        m_id = other.m_id;
        m_registers = other.m_registers;
        m_running = other.m_running.load();
        if (other.m_thread.joinable()) {
            m_thread = std::move(other.m_thread);
        }
    }
    return *this;
}

void ProcessingElement::loadProgram(const std::vector<Instruction>& prog) {
    std::scoped_lock lock(m_regMutex);
    m_program = prog;
    m_pc = 0;
}

void ProcessingElement::attachMemory(SharedMemory* mem) {
    m_mem = mem;
}

void ProcessingElement::start(ThreadFunc func) {
    if (m_running.load()) {
        throw std::runtime_error("ProcessingElement already running");
    }
    m_running = true;
    if (func) {
        // custom function overrides default execution
        m_thread = std::thread([this, func]() {
            func(*this);
            m_running = false;
        });
    } else {
        // default: execute loaded program
        m_thread = std::thread([this]() {
            while (m_running.load()) {
                Instruction inst;
                {
                    std::scoped_lock lock(m_regMutex);
                    if (m_pc >= m_program.size()) { m_running = false; break; }
                    inst = m_program[m_pc];
                }
                switch (inst.op) {
                    case OpCode::LOAD: {
                        uint64_t val = m_mem ? m_mem->load(inst.addr) : 0;
                        writeReg(inst.rd, val);
                        m_pc++; break;
                    }
                    case OpCode::STORE: {
                        uint64_t val = readReg(inst.rd);
                        if (m_mem) m_mem->store(inst.addr, val);
                        m_pc++; break;
                    }
                    case OpCode::FMUL: {
                        uint64_t aBits = readReg(inst.ra);
                        uint64_t bBits = readReg(inst.rb);
                        double a, b; std::memcpy(&a, &aBits, sizeof(uint64_t)); std::memcpy(&b, &bBits, sizeof(uint64_t));
                        double r = a * b;
                        uint64_t raw; std::memcpy(&raw, &r, sizeof(uint64_t));
                        writeReg(inst.rd, raw);
                        m_pc++; break;
                    }
                    case OpCode::FADD: {
                        uint64_t aBits = readReg(inst.ra);
                        uint64_t bBits = readReg(inst.rb);
                        double a, b; std::memcpy(&a, &aBits, sizeof(uint64_t)); std::memcpy(&b, &bBits, sizeof(uint64_t));
                        double r = a + b;
                        uint64_t raw; std::memcpy(&raw, &r, sizeof(uint64_t));
                        writeReg(inst.rd, raw);
                        m_pc++; break;
                    }
                    case OpCode::INC: {
                        addImm(inst.rd, 1);
                        m_pc++; break;
                    }
                    case OpCode::DEC: {
                        addImm(inst.rd, static_cast<uint64_t>(-1));
                        m_pc++; break;
                    }
                    case OpCode::JNZ: {
                        uint64_t cond = readReg(7);
                        if (cond != 0) m_pc = inst.target; else m_pc++;
                        break;
                    }
                    case OpCode::HALT: {
                        m_running = false; break;
                    }
                }
            }
            m_running = false;
        });
    }
}

void ProcessingElement::join() {
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

uint64_t ProcessingElement::readReg(size_t idx) const {
    if (idx >= REG_COUNT) throw std::out_of_range("Invalid register index");
    std::scoped_lock lock(m_regMutex);
    return m_registers[idx];
}

void ProcessingElement::writeReg(size_t idx, uint64_t value) {
    if (idx >= REG_COUNT) throw std::out_of_range("Invalid register index");
    std::scoped_lock lock(m_regMutex);
    m_registers[idx] = value;
}

void ProcessingElement::addImm(size_t dstIdx, uint64_t imm) {
    if (dstIdx >= REG_COUNT) throw std::out_of_range("Invalid register index");
    std::scoped_lock lock(m_regMutex);
    m_registers[dstIdx] += imm;
}
