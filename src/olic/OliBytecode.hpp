#ifndef OLIBYTECODE_HPP
#define OLIBYTECODE_HPP

#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include "../vData.hpp"

enum class OpCode : uint8_t {
    OP_RETURN,          // 0x00
    OP_CONSTANT,        // 0x01
    OP_SET_GLOBAL,      // 0x02
    OP_GET_GLOBAL,      // 0x03
    OP_ADD,             // 0x04
    OP_SUB,             // 0x05
    OP_MUL,             // 0x06
    OP_DIV,             // 0x07
    OP_ECHO,            // 0x08
    OP_CALL_PLUGIN_CMD, // 0x09
    OP_JUMP_IF_FALSE,   // 0x0A
    OP_JUMP,            // 0x0B
    OP_GREATER,         // 0x0C
    OP_LESS,            // 0x0D
    OP_EQUAL,           // 0x0E
    OP_LOOP,            // 0x0F
    OP_DUP,             // 0x10
    OP_GET_INDIRECT,    // 0x11
    OP_SET_INDIRECT,    // 0x12
    OP_JUMP_IF_TRUE     // 0x13
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
        case OpCode::OP_CONSTANT: {
            uint16_t idx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_CONSTANT     " << std::setw(4) << idx
                << L" (Value: " << chunk.constants[idx].toWString() << L")\n";
            break;
        }
        case OpCode::OP_SET_GLOBAL: {
            uint16_t idx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_SET_GLOBAL   " << std::setw(4) << idx
                << L" (Name: " << chunk.constants[idx].toWString() << L")\n";
            break;
        }
        case OpCode::OP_GET_GLOBAL: {
            uint16_t idx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_GET_GLOBAL   " << std::setw(4) << idx
                << L" (Name: " << chunk.constants[idx].toWString() << L")\n";
            break;
        }
        case OpCode::OP_ECHO:    ss << L"OP_ECHO\n"; break;
        case OpCode::OP_ADD:     ss << L"OP_ADD\n"; break;
        case OpCode::OP_SUB:     ss << L"OP_SUB\n"; break;
        case OpCode::OP_MUL:     ss << L"OP_MUL\n"; break;
        case OpCode::OP_DIV:     ss << L"OP_DIV\n"; break;
        case OpCode::OP_RETURN:  ss << L"OP_RETURN\n"; break;
        case OpCode::OP_GREATER: ss << L"OP_GREATER\n"; break;
        case OpCode::OP_LESS:    ss << L"OP_LESS\n"; break;    // <-- FIX
        case OpCode::OP_EQUAL:   ss << L"OP_EQUAL\n"; break;   // <-- FIX
        case OpCode::OP_DUP:     ss << L"OP_DUP\n"; break;      // <-- FIX
        case OpCode::OP_JUMP_IF_FALSE: {
            uint16_t offset = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_JUMP_IF_FALSE " << std::setw(4) << offset
                << L" (Sari la adresa: " << (ip + offset) << L")\n";
            break;
        }
        case OpCode::OP_JUMP: {
            uint16_t offset = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_JUMP          " << std::setw(4) << offset
                << L" (Sari la adresa: " << (ip + offset) << L")\n";
            break;
        }
        case OpCode::OP_LOOP: {
            uint16_t offset = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_LOOP          " << std::setw(4) << offset
                << L" (Sari inapoi la: " << (ip - offset) << L")\n";
            break;
        }
        case OpCode::OP_GET_INDIRECT:
            ss << L"OP_GET_INDIRECT\n";
            break;
        case OpCode::OP_SET_INDIRECT:
            ss << L"OP_SET_INDIRECT\n";
            break;
        case OpCode::OP_JUMP_IF_TRUE: {
            // Folosim ip-ul global din loop-ul de dezasamblare
            uint16_t offset = (uint16_t)(chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ss << L"OP_JUMP_IF_TRUE   " << std::setw(4) << std::setfill(L'0') << offset
                << L" (Sari la adresa: " << (ip + offset) << L")\n";
            break;
        }
        default:
            ss << L"UNKNOWN_OP [0x" << std::hex << (int)instruction << std::dec << L"]\n";
            break;
        }
    }
    return ss.str();
}

#endif