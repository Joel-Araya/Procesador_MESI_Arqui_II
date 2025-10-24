#pragma once
#include "ProcessingElement.hpp"
#include "Instruction.hpp"
#include <array>
#include <vector>
#include <functional>

class ProcessorSystem {
public:
    static constexpr size_t PE_COUNT = 4;

    ProcessorSystem(bool debug = false);

    ProcessingElement& getPE(size_t idx);
    const ProcessingElement& getPE(size_t idx) const;

    // Launch all PEs with provided function (receives reference to its PE instance)
    void startAll(const std::function<void(ProcessingElement&)>& func);

    // Provide a vector of functions (size must be PE_COUNT)
    void startAll(const std::vector<std::function<void(ProcessingElement&)>>& funcs);

    void joinAll();

    // Cargar un programa en un PE específico
    void loadProgram(size_t peIdx, const std::vector<Instruction>& prog);

    // Cargar programas para todos los PEs (array size == PE_COUNT)
    void loadPrograms(const std::array<std::vector<Instruction>, PE_COUNT>& programs);

private:
    std::array<ProcessingElement, PE_COUNT> m_pes;
};
