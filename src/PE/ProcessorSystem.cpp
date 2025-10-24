#include "ProcessorSystem.hpp"
#include "Instruction.hpp"
#include <stdexcept>

ProcessorSystem::ProcessorSystem(bool debug) : m_pes{ ProcessingElement(0, debug), ProcessingElement(1, debug), ProcessingElement(2, debug), ProcessingElement(3, debug) } {}

ProcessingElement& ProcessorSystem::getPE(size_t idx) {
    if (idx >= PE_COUNT) throw std::out_of_range("Invalid PE index");
    return m_pes[idx];
}

const ProcessingElement& ProcessorSystem::getPE(size_t idx) const {
    if (idx >= PE_COUNT) throw std::out_of_range("Invalid PE index");
    return m_pes[idx];
}

void ProcessorSystem::startAll(const std::function<void(ProcessingElement&)>& func) {
    for (auto & pe : m_pes) {
        pe.start(func);
    }
}

void ProcessorSystem::startAll(const std::vector<std::function<void(ProcessingElement&)>>& funcs) {
    if (funcs.size() != PE_COUNT) throw std::invalid_argument("Function vector size mismatch");
    for (size_t i = 0; i < PE_COUNT; ++i) {
        m_pes[i].start(funcs[i]);
    }
}

void ProcessorSystem::joinAll() {
    for (auto & pe : m_pes) {
        pe.join();
    }
}

void ProcessorSystem::loadProgram(size_t peIdx, const std::vector<Instruction>& prog) {
    if (peIdx >= PE_COUNT) throw std::out_of_range("Invalid PE index");
    m_pes[peIdx].loadProgram(prog);
}

void ProcessorSystem::loadPrograms(const std::array<std::vector<Instruction>, PE_COUNT>& programs) {
    for (size_t i = 0; i < PE_COUNT; ++i) {
        m_pes[i].loadProgram(programs[i]);
    }
}
