#include "ProgramLoader.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <stdexcept>
#include <cctype>

static int regIndex(const std::string& tok) {
    if (tok.size() < 2 || (tok[0] != 'R' && tok[0] != 'r')) throw std::runtime_error("Registro invalido: " + tok);
    int idx = std::stoi(tok.substr(1));
    if (idx < 0 || idx > 7) throw std::runtime_error("Indice de registro fuera de rango: " + tok);
    return idx;
}

std::vector<Instruction> loadProgramFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("No se pudo abrir archivo: " + path);
    std::string line;
    std::vector<Instruction> program;
    std::unordered_map<std::string,size_t> labelPos;
    struct Fixup { size_t instrIndex; std::string label; };
    std::vector<Fixup> fixups;

    size_t lineNum = 0;
    while (std::getline(in,line)) {
        lineNum++;
        // quitar comentarios
        auto sc = line.find(';'); if (sc!=std::string::npos) line = line.substr(0,sc);
        auto hc = line.find('#'); if (hc!=std::string::npos) line = line.substr(0,hc);
        // trim
        auto trim = [](std::string& s){
            while(!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
            while(!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
        };
        trim(line);
        if (line.empty()) continue;
        // label?
        if (line.back()==':') {
            std::string label = line.substr(0,line.size()-1);
            trim(label);
            if (label.empty()) throw std::runtime_error("Etiqueta vacia linea " + std::to_string(lineNum));
            labelPos[label] = program.size();
            continue;
        }
        std::istringstream iss(line);
        std::string op; iss >> op;
        for (auto & c: op) c = std::toupper((unsigned char)c);
        if (op == "LOAD") {
            std::string rdComma, addrStr; iss >> rdComma; // formato Rn,
            if (rdComma.back()==',') rdComma.pop_back();
            iss >> addrStr;
            uint64_t addr = std::stoull(addrStr, nullptr, 0);
            program.push_back({OpCode::LOAD, regIndex(rdComma), -1, -1, addr});
        } else if (op == "STORE") {
            std::string rsComma, addrStr; iss >> rsComma; if (rsComma.back()==',') rsComma.pop_back();
            iss >> addrStr; uint64_t addr = std::stoull(addrStr, nullptr, 0);
            program.push_back({OpCode::STORE, regIndex(rsComma), -1, -1, addr});
        } else if (op == "INC") {
            std::string r; iss >> r; program.push_back({OpCode::INC, regIndex(r)});
        } else if (op == "DEC") {
            std::string r; iss >> r; program.push_back({OpCode::DEC, regIndex(r)});
        } else if (op == "FADD" || op == "FMUL") {
            std::string rd, ra, rb; iss >> rd; if (rd.back()==',') rd.pop_back(); iss >> ra; if (ra.back()==',') ra.pop_back(); iss >> rb; 
            Instruction inst; inst.op = (op=="FADD"?OpCode::FADD:OpCode::FMUL); inst.rd = regIndex(rd); inst.ra = regIndex(ra); inst.rb = regIndex(rb);
            program.push_back(inst);
        } else if (op == "JNZ") {
            std::string label; iss >> label; Instruction inst; inst.op = OpCode::JNZ; inst.target = 0; program.push_back(inst); fixups.push_back({program.size()-1,label});
        } else if (op == "HALT") {
            program.push_back({OpCode::HALT});
        } else if (op == "MOVI") {
            std::string rdComma, immStr; iss >> rdComma; if (rdComma.back()==',') rdComma.pop_back(); iss >> immStr; uint64_t imm = std::stoull(immStr, nullptr, 0);
            program.push_back({OpCode::MOVI, regIndex(rdComma), -1, -1, imm});
        } else if (op == "ADDI") {
            std::string rdComma, immStr; iss >> rdComma; if (rdComma.back()==',') rdComma.pop_back(); iss >> immStr; uint64_t imm = std::stoull(immStr, nullptr, 0);
            program.push_back({OpCode::ADDI, regIndex(rdComma), -1, -1, imm});
        } else if (op == "ADD") {
            std::string rd, ra, rb; iss >> rd; if (rd.back()==',') rd.pop_back(); iss >> ra; if (ra.back()==',') ra.pop_back(); iss >> rb;
            Instruction inst; inst.op = OpCode::ADD; inst.rd = regIndex(rd); inst.ra = regIndex(ra); inst.rb = regIndex(rb); program.push_back(inst);
        } else if (op == "LOADR") {
            std::string rd, ra; iss >> rd; if (rd.back()==',') rd.pop_back(); iss >> ra; // ra is address register
            Instruction inst; inst.op = OpCode::LOADR; inst.rd = regIndex(rd); inst.ra = regIndex(ra); program.push_back(inst);
        } else if (op == "STORER") {
            std::string rs, ra; iss >> rs; if (rs.back()==',') rs.pop_back(); iss >> ra; // ra is address register
            Instruction inst; inst.op = OpCode::STORER; inst.rd = regIndex(rs); inst.ra = regIndex(ra); program.push_back(inst);
        } else {
            throw std::runtime_error("Operacion desconocida linea " + std::to_string(lineNum) + ": " + op);
        }
    }
    // resolver fixups
    for (auto & f : fixups) {
        auto it = labelPos.find(f.label);
        if (it == labelPos.end()) throw std::runtime_error("Etiqueta no encontrada: " + f.label);
        program[f.instrIndex].target = it->second;
    }
    return program;
}
