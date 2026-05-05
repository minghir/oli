#include "vDataSerialize.hpp"
#include "OliEngine.hpp"
#include "olic/OliBytecode.hpp"

void vOliEngine::executeBytecode(const OliChunk& chunk) {
    size_t ip = 0; // Instruction Pointer (unde ne aflăm în cod)
    std::vector<vData> stack; // Stiva pentru calcule

    while (ip < chunk.code.size()) {
        OpCode instruction = static_cast<OpCode>(chunk.code[ip++]);

        switch (instruction) {
        case OpCode::OP_CONSTANT: {
            // Citim indexul constantei (2 bytes)
            uint16_t idx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            stack.push_back(chunk.constants[idx]);
            break;
        }

        case OpCode::OP_SET_GLOBAL: {
            vData val = stack.back();
            stack.pop_back();

            uint16_t nameIdx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;

            // Folosim getTrueData() pentru a fi siguri că nu lucrăm cu un pointer la nume
            vData nameConst = chunk.constants[nameIdx].getTrueData();
            if (!nameConst.isString()) {
                this->logError(L"VM Error: OP_SET_GLOBAL target is not a string name.");
                break;
            }

            std::wstring varName = std::get<std::wstring>(nameConst.value);
            this->setVar(varName, val);
            break;
        }

        case OpCode::OP_ECHO: {
            if (stack.empty()) break;
            vData val = stack.back();
            stack.pop_back();

            // Acum vDataSerialize::stringify funcționează deoarece am creat namespace-ul
            std::wstring out = vDataSerialize::stringify(val);

            std::wcout << out << std::endl;
            std::wcout.flush(); // Siguranță pentru interfață interactivă
            break;
        }

        case OpCode::OP_RETURN:
            return;

        

     

        case OpCode::OP_GREATER: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            // Convertim în double pentru o comparare universală (merge și pt int și pt float)
            double valA = a.isInt() ? (double)std::get<long long>(a.value) : std::get<double>(a.value);
            double valB = b.isInt() ? (double)std::get<long long>(b.value) : std::get<double>(b.value);

            // Împingem rezultatul logic (bool) înapoi pe stivă
            stack.push_back(vData(valA > valB));
            break;
        }

        case OpCode::OP_JUMP_IF_FALSE: {
            uint16_t offset = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2; // Trecem peste offset

            vData condition = stack.back();
            stack.pop_back();

            if (std::get<bool>(condition.value) == false) {
                ip += offset; // SĂRIM peste corpul IF-ului
            }
            break;
        }
        case OpCode::OP_JUMP: {
            uint16_t offset = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ip += offset; // Sarim mereu
            break;
        }
        case OpCode::OP_LOOP: {
            // Citim offset-ul de 2 bytes
            uint16_t offset = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2; // Trecem peste offset

            // Saltul înapoi: scădem offset-ul din Instruction Pointer
            ip -= offset;
            break;
        }

        case OpCode::OP_LESS: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            double valA = a.isInt() ? (double)std::get<long long>(a.value) : std::get<double>(a.value);
            double valB = b.isInt() ? (double)std::get<long long>(b.value) : std::get<double>(b.value);
            stack.push_back(vData(valA < valB));
            break;
        }

        case OpCode::OP_EQUAL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            // Aici poți folosi direct operatorul de egalitate al vData dacă l-ai supraîncărcat
            stack.push_back(vData(a.toWString() == b.toWString()));
            break;
        }

        case OpCode::OP_SUB: {
            // Prima valoare scoasă de pe stivă este 'b' (scăzătorul)
            vData b = stack.back(); stack.pop_back();
            // A doua valoare scoasă este 'a' (descăzutul)
            vData a = stack.back(); stack.pop_back();

            LOG_DEBUG(L"SUB: " + a.toWString() + L" - " + b.toWString());

            if (a.isInt() && b.isInt()) {
                // Dacă ambii sunt întregi, rezultatul rămâne întreg
                stack.push_back(vData(std::get<long long>(a.value) - std::get<long long>(b.value)));
            }
            else {
                try {
                    // Conversie la double pentru operații cu virgulă sau mixte
                    double valA = a.isInt() ? (double)std::get<long long>(a.value) : std::get<double>(a.value);
                    double valB = b.isInt() ? (double)std::get<long long>(b.value) : std::get<double>(b.value);
                    stack.push_back(vData(valA - valB));
                }
                catch (...) {
                    this->logError(L"Crash la scădere! Tipuri incompatibile.");
                    return;
                }
            }
            break;
        }
        case OpCode::OP_DUP: {
            if (stack.empty()) {
                this->logError(L"Stack underflow la OP_DUP!");
                return;
            }
            // Punem pe stivă o copie a ultimului element
            stack.push_back(stack.back());
            break;
        }
        

        
        case OpCode::OP_JUMP_IF_TRUE: {
            // 1. Citim offset-ul (2 bytes)
            uint16_t offset = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;

            // 2. Evaluăm condiția de pe stivă
            vData condition = stack.back();
            stack.pop_back();

            // 3. Dacă e TRUE, aplicăm saltul (ieșim din REPEAT)
            if (vDataToBool(condition)) {
                ip += offset;
            }
            break;
        }
        
        case OpCode::OP_GET_ADDR: {
            if (ip + 1 >= chunk.code.size()) return;
            uint16_t nameIdx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;

            std::wstring rawName = chunk.constants[nameIdx].toWString();
            std::wstring cleanName = this->cleanVariableName(rawName);

            vData* targetPtr = nullptr;

            // Căutăm în Stack-ul local (dacă există) sau în Globale
            if (!m_callStack.empty()) {
                auto& locals = m_callStack.back().localVariables;
                if (locals.count(cleanName)) targetPtr = &locals[cleanName];
            }

            if (!targetPtr) {
                if (m_globalVariables.count(cleanName)) {
                    targetPtr = &m_globalVariables[cleanName];
                }
                else {
                    // Auto-creare dacă variabila nu există
                    m_globalVariables[cleanName] = { std::monostate{} };
                    targetPtr = &m_globalVariables[cleanName];
                }
            }

            if (targetPtr) {
                vData addr;
                addr.value = targetPtr; // Stocăm pointerul C++ direct
                stack.push_back(addr);
            }
            break;
        }

        case OpCode::OP_MUL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(vDataToDouble(a) * vDataToDouble(b)));
            break;
        }

        case OpCode::OP_DIV: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            double valB = vDataToDouble(b);
            if (valB == 0) {
                this->logError(L"Runtime Error: Division by zero!");
                return;
            }
            stack.push_back(vData(vDataToDouble(a) / valB));
            break;
        }

        case OpCode::OP_POW: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(std::pow(vDataToDouble(a), vDataToDouble(b))));
            break;
        }

        case OpCode::OP_NULL_COALESCE: { // 0x20
            vData rhs = stack.back(); stack.pop_back();
            vData lhs = stack.back(); stack.pop_back();
            stack.push_back(!lhs.isNull() ? lhs : rhs);
            break;
        }

        case OpCode::OP_CONCAT: { // 0x22
            vData rhs = stack.back(); stack.pop_back();
            vData lhs = stack.back(); stack.pop_back();
            stack.push_back(vData(lhs.toWString() + rhs.toWString()));
            break;
        }
        
        case OpCode::OP_GET_GLOBAL: {
            if (ip + 1 >= chunk.code.size()) return;
            uint16_t nameIdx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;

            std::wstring varName = chunk.constants[nameIdx].toWString();

            // Obținem variabila (care în proiectul 'olish' poate fi un Map/Ierarhie)
            vData val = this->getVar(varName);

            // FIX: Folosim getScalarValue() pentru a trece de Map-ul ierarhiei 
            // și a pune pe stivă valoarea reală (sau pointerul real).
            stack.push_back(val.getFlattenedValue());
            break;
        }

        case OpCode::OP_ADD: {
            if (stack.size() < 2) {
                this->logError(L"Stack Underflow la OP_ADD! Stiva e aproape goală.");
                return;
            }

            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            // Folosim getTrueData() abia ACUM. 
            // Dacă 'a' sau 'b' sunt pointeri, getTrueData() va săpa după valorile reale (42, 8).
            vData realA = a.getTrueData();
            vData realB = b.getTrueData();

            if (realA.isInt() && realB.isInt()) {
                stack.push_back(vData(std::get<long long>(realA.value) + std::get<long long>(realB.value)));
            }
            else {
                // Conversie sigură la double pentru orice altceva
                double valA = vDataToDouble(realA);
                double valB = vDataToDouble(realB);
                stack.push_back(vData(valA + valB));
            }
            break;
        }

        case OpCode::OP_GET_INDIRECT: {
            if (stack.empty()) return;
            vData container = stack.back();
            stack.pop_back();

            // 1. Încercăm să vedem dacă avem un pointer direct (vData*)
            if (vData** ptrPtr = std::get_if<vData*>(&container.value)) {
                if (*ptrPtr) {
                    // Săpăm prin pointer până la date, apoi extragem scalarul din ierarhie
                    stack.push_back((*ptrPtr)->getTrueData().getScalarValue());
                }
                else {
                    stack.push_back({ std::monostate{} });
                }
            }
            // 2. Altfel, tratăm ca indirație prin nume (string) tip $$nume
            else {
                std::wstring varName = container.getScalarValue().toWString();
                // Căutăm variabila și îi extragem valoarea scalară
                stack.push_back(this->getVar(varName).getScalarValue());
            }
            break;
        }

        case OpCode::OP_SET_INDIRECT: {
            if (stack.size() < 2) {
                this->logError(L"Stack Underflow la OP_SET_INDIRECT! Stiva este goală.");
                return;
            }

            // Ordinea corectă: Target-ul (adresa) este ultima pusă pe stivă de compilator
            vData target = stack.back(); stack.pop_back();   // Scoatem adresa (sau numele)
            vData newValue = stack.back(); stack.pop_back(); // Scoatem valoarea de scris

            if (vData** ptrPtr = std::get_if<vData*>(&target.value)) {
                if (*ptrPtr) {
                    // Scriem în memoria reală folosind logica ta de dereferențiere
                    (*ptrPtr)->getTrueData().value = newValue.getTrueData().value;
                }
            }
            else {
                // Caz de indirație prin nume ($$a)
                std::wstring name = target.getScalarValue().toWString();
                this->setVar(name, newValue);
            }
            break;
        }

                                    // --- OPERAȚII DE STIVĂ ---
        case OpCode::OP_POP: {
            if (!stack.empty()) stack.pop_back();
            break;
        }

                           // --- ARITMETICĂ ---
        case OpCode::OP_MOD: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            // Modulo funcționează de regulă pe întregi
            long long valA = a.getTrueData().toInt();
            long long valB = b.getTrueData().toInt();

            if (valB == 0) {
                this->logError(L"Runtime Error: Modulo by zero!");
                return;
            }
            stack.push_back(vData(valA % valB));
            break;
        }

        case OpCode::OP_NEGATE: {
            vData val = stack.back();
            stack.pop_back();

            if (val.isInt()) {
                stack.push_back(vData(-std::get<long long>(val.getTrueData().value)));
            }
            else {
                stack.push_back(vData(-vDataToDouble(val.getTrueData())));
            }
            break;
        }

                              // --- COMPARAȚII SUPLIMENTARE ---
        case OpCode::OP_NOT_EQUAL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(a.toWString() != b.toWString()));
            break;
        }

        case OpCode::OP_GREATER_EQUAL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(vDataToDouble(a.getTrueData()) >= vDataToDouble(b.getTrueData())));
            break;
        }

        case OpCode::OP_LESS_EQUAL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(vDataToDouble(a.getTrueData()) <= vDataToDouble(b.getTrueData())));
            break;
        }



        default:
            this->logError(L"Unknown OpCode in VM!");
            return;
        }
    }
}


void vOliEngine::loadAndRunBytecode(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        this->logError(L"Could not open bytecode file: " + str_to_wstr(path));
        return;
    }

    OliChunk chunk;
    uint32_t constCount = 0;
    uint32_t codeSize = 0;

    // 1. Citim numărul de constante
    ifs.read(reinterpret_cast<char*>(&constCount), sizeof(constCount));

    // --- AICI DESERIALIZEZI ---
    // Reconstruim tabelul de constante citind fiecare vData din fișier
    for (uint32_t i = 0; i < constCount; ++i) {
        chunk.constants.push_back(vDataSerialize::deserializevData(ifs));
    }

    // 2. Citim dimensiunea codului
    ifs.read(reinterpret_cast<char*>(&codeSize), sizeof(codeSize));

    // 3. Citim codul binar
    chunk.code.resize(codeSize);
    ifs.read(reinterpret_cast<char*>(chunk.code.data()), codeSize);

    ifs.close();

    // 4. Executăm!
    this->executeBytecode(chunk);
}