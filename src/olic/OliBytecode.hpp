#ifndef OLIBYTECODE_HPP
#define OLIBYTECODE_HPP

#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include "../vData.hpp"


enum class OpCode : uint8_t {
    // --- MEMORIE & STIVĂ ---
    OP_CONSTANT, 
    OP_SET_GLOBAL, 
    OP_GET_GLOBAL,
    OP_SET_INDIRECT, 
    OP_GET_INDIRECT,
    OP_DUP, 
    OP_POP,
    OP_GET_ADDR,

    // --- ARITMETICĂ BINARĂ ---
    OP_ADD, 
    OP_SUB, 
    OP_MUL, 
    OP_DIV, 
    OP_MOD,
    OP_POW,             // ** (Putere/Exponentiere)

    // --- ARITMETICĂ UNARĂ ---
    OP_INC,             // ++
    OP_DEC,             // --
    OP_NEGATE,          // - (Minus unar)

    // --- BITWISE (Nivel Jos) ---
    OP_BAND, 
    OP_BOR, 
    OP_BXOR, 
    OP_BNOT,
    OP_SHL, 
    OP_SHR,

    // --- LOGICĂ (Scurtcircuitare) ---
    OP_LOGICAL_AND,     // &&
    OP_LOGICAL_OR,      // ||
    OP_LOGICAL_NOT,     // !

    // --- COMPARAȚIE ---
    OP_EQUAL, 
    OP_NOT_EQUAL,
    OP_GREATER, 
    OP_GREATER_EQUAL,
    OP_LESS, 
    OP_LESS_EQUAL,

    // --- OPERATORI SPECIALI (Pentru "Super" Oli) ---
    OP_NULL_COALESCE,   // ?? (Dacă e null, ia valoarea de rezervă)
    OP_TYPEOF,          // TYPE() - Returnează tipul variabilei
    OP_CONCAT,          // .. sau + pentru concatenare string-uri

    // --- CONTROL FLOW ---
    OP_JUMP, 
    OP_JUMP_IF_TRUE, 
    OP_JUMP_IF_FALSE,
    OP_LOOP, 
    OP_CALL, 
    OP_RETURN,

    // --- SYSTEM ---
    OP_ECHO, 
    OP_HALT,

    //OP_NULL_COALESCE,
    //OP_CONCAT
};


struct OliChunk {
    std::vector<uint8_t> code;
    std::vector<vData> constants;
    std::vector<int> lines;

    void addByte(uint8_t b, int line) {
        code.push_back(b);
        lines.push_back(line);
    }

    uint16_t addConstant(vData value) {
        for (uint16_t i = 0; i < (uint16_t)constants.size(); ++i) {
            if (constants[i] == value) return i;
        }
        constants.push_back(value);
        return (uint16_t)(constants.size() - 1);
    }
};

inline std::wstring disassembleChunk(const OliChunk& chunk) {
    std::wstringstream ss;
    size_t ip = 0;

    while (ip < chunk.code.size()) {
        ss << std::setw(4) << std::setfill(L'0') << ip << L": ";
        uint8_t instruction = chunk.code[ip++];
        OpCode op = static_cast<OpCode>(instruction);

        switch (op) {
            // --- MEMORIE & STIVĂ ---
        case OpCode::OP_CONSTANT: {
            uint16_t idx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_CONSTANT     " << std::setw(4) << idx << L" (Value: " << chunk.constants[idx].toWString() << L")\n";
            break;
        }
        case OpCode::OP_SET_GLOBAL: {
            uint16_t idx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_SET_GLOBAL   " << std::setw(4) << idx << L" (Name: " << chunk.constants[idx].toWString() << L")\n";
            break;
        }
        case OpCode::OP_GET_GLOBAL: {
            uint16_t idx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_GET_GLOBAL   " << std::setw(4) << idx << L" (Name: " << chunk.constants[idx].toWString() << L")\n";
            break;
        }
        case OpCode::OP_GET_ADDR: {
            uint16_t idx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_GET_ADDR      " << std::setw(4) << idx << L" (Variable: " << chunk.constants[idx].toWString() << L")\n";
            break;
        }
        case OpCode::OP_SET_INDIRECT: ss << L"OP_SET_INDIRECT\n"; break;
        case OpCode::OP_GET_INDIRECT: ss << L"OP_GET_INDIRECT\n"; break;
        case OpCode::OP_DUP:          ss << L"OP_DUP\n"; break;
        case OpCode::OP_POP:          ss << L"OP_POP\n"; break;

            // --- ARITMETICĂ ---
        case OpCode::OP_ADD:    ss << L"OP_ADD\n"; break;
        case OpCode::OP_SUB:    ss << L"OP_SUB\n"; break;
        case OpCode::OP_MUL:    ss << L"OP_MUL\n"; break;
        case OpCode::OP_DIV:    ss << L"OP_DIV\n"; break;
        case OpCode::OP_MOD:    ss << L"OP_MOD\n"; break;
        case OpCode::OP_POW:    ss << L"OP_POW\n"; break;
        case OpCode::OP_NEGATE: ss << L"OP_NEGATE\n"; break;
        case OpCode::OP_INC:    ss << L"OP_INC\n"; break;
        case OpCode::OP_DEC:    ss << L"OP_DEC\n"; break;

            // --- BITWISE ---
        case OpCode::OP_BAND:   ss << L"OP_BAND\n"; break;
        case OpCode::OP_BOR:    ss << L"OP_BOR\n"; break;
        case OpCode::OP_BXOR:   ss << L"OP_BXOR\n"; break;
        case OpCode::OP_BNOT:   ss << L"OP_BNOT\n"; break;
        case OpCode::OP_SHL:    ss << L"OP_SHL\n"; break;
        case OpCode::OP_SHR:    ss << L"OP_SHR\n"; break;

            // --- LOGICĂ & COMPARAȚIE ---
        case OpCode::OP_LOGICAL_AND: ss << L"OP_LOGICAL_AND\n"; break;
        case OpCode::OP_LOGICAL_OR:  ss << L"OP_LOGICAL_OR\n"; break;
        case OpCode::OP_LOGICAL_NOT: ss << L"OP_LOGICAL_NOT\n"; break;
        case OpCode::OP_EQUAL:       ss << L"OP_EQUAL\n"; break;
        case OpCode::OP_NOT_EQUAL:   ss << L"OP_NOT_EQUAL\n"; break;
        case OpCode::OP_GREATER:     ss << L"OP_GREATER\n"; break;
        case OpCode::OP_GREATER_EQUAL: ss << L"OP_GREATER_EQUAL\n"; break;
        case OpCode::OP_LESS:        ss << L"OP_LESS\n"; break;
        case OpCode::OP_LESS_EQUAL:    ss << L"OP_LESS_EQUAL\n"; break;

            // --- SPECIALI ---
        case OpCode::OP_NULL_COALESCE: ss << L"OP_NULL_COALESCE\n"; break;
        case OpCode::OP_CONCAT:        ss << L"OP_CONCAT\n"; break;
        case OpCode::OP_TYPEOF:        ss << L"OP_TYPEOF\n"; break;

            // --- CONTROL FLOW ---
        case OpCode::OP_JUMP: {
            uint16_t offset = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_JUMP          " << std::setw(4) << offset << L" (Sari la: " << (ip + offset) << L")\n";
            break;
        }
        case OpCode::OP_JUMP_IF_FALSE: {
            uint16_t offset = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_JUMP_IF_FALSE " << std::setw(4) << offset << L" (Sari la: " << (ip + offset) << L")\n";
            break;
        }
        case OpCode::OP_JUMP_IF_TRUE: {
            uint16_t offset = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_JUMP_IF_TRUE  " << std::setw(4) << offset << L" (Sari la: " << (ip + offset) << L")\n";
            break;
        }
        case OpCode::OP_LOOP: {
            uint16_t offset = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_LOOP          " << std::setw(4) << offset << L" (Sari inapoi la: " << (ip - offset) << L")\n";
            break;
        }
        case OpCode::OP_CALL:   ss << L"OP_CALL\n"; break;
        case OpCode::OP_RETURN: ss << L"OP_RETURN\n"; break;

            // --- SYSTEM ---
        case OpCode::OP_ECHO:   ss << L"OP_ECHO\n"; break;
        case OpCode::OP_HALT:   ss << L"OP_HALT\n"; break;

        default:
            ss << L"UNKNOWN_OP [0x" << std::hex << (int)instruction << std::dec << L"]\n";
            break;
        }
    }
    return ss.str();
}

#endif